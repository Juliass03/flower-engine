#include "material.h"
#include "mesh.h"
#include "texture.h"
#include "renderer.h"
#include <filesystem>
#include <future>

using namespace engine;

MaterialInfoCache* engine::MaterialInfoCache::s_cache = new MaterialInfoCache();

void engine::Material::release()
{
	releaseDepthResource();
	releaseGbufferResource();

	for(auto& pipeline : m_outOfDayPipeline)
	{
		if(pipeline!=VK_NULL_HANDLE)
		{
			vkDestroyPipeline(VulkanRHI::get()->getDevice(),pipeline,nullptr);
		}
		pipeline = VK_NULL_HANDLE;
	}

	m_outOfDayPipeline.clear();
	m_outOfDayPipeline.resize(0);
}

void engine::Material::releaseDepthResource()
{
	if(m_depthPipeline != VK_NULL_HANDLE)
	{
		m_outOfDayPipeline.push_back(m_depthPipeline);
		m_depthPipeline = VK_NULL_HANDLE;
	}
}

void engine::Material::releaseGbufferResource()
{
	if(m_gbufferPipeline!=VK_NULL_HANDLE)
	{
		m_outOfDayPipeline.push_back(m_gbufferPipeline);
		m_gbufferPipeline = VK_NULL_HANDLE;
	}
}

void engine::Material::getMaterialDescriptorSet(const MeshPassLayout& pass_layout,bool bRebuildMaterial)
{
	buildMeshDescriptorSet(m_shaderCompiler,pass_layout,bRebuildMaterial); // NOTE: 纹理发生变化时将会在这里重建
}

bool engine::Material::allShaderInfoReady()
{
	return m_shaderInfo && m_depthShaderInfo;
}

void fillPipelineBuilder(VulkanGraphicsPipelineFactory& inout,shaderCompiler::EShaderPass type)
{
	if(type==shaderCompiler::EShaderPass::GBuffer)
	{
		VulkanVertexInputDescription vvid = {};
		vvid.bindings =  { VulkanVertexBuffer::getInputBinding(getStandardMeshAttributes()) };
		vvid.attributes = VulkanVertexBuffer::getInputAttribute(getStandardMeshAttributes());
		inout.vertexInputDescription = vvid;
		inout.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		inout.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		inout.rasterizer.cullMode = VK_CULL_MODE_NONE;
		inout.multisampling = vkMultisamplingStateCreateInfo();
		inout.colorBlendAttachments.push_back(vkColorBlendAttachmentState());
		inout.colorBlendAttachments.push_back(vkColorBlendAttachmentState());
		inout.colorBlendAttachments.push_back(vkColorBlendAttachmentState());
		inout.depthStencil = vkDepthStencilCreateInfo(true,true,getEngineZTestFunc());
	}
	else if(type==shaderCompiler::EShaderPass::ShadowDepth)
	{
		VulkanVertexInputDescription vvid = {};
		vvid.bindings =  { VulkanVertexBuffer::getInputBinding(getStandardMeshAttributes()) };
		vvid.attributes = VulkanVertexBuffer::getInputAttribute(getStandardMeshAttributes());
		inout.vertexInputDescription = vvid;
		inout.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		inout.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		inout.rasterizer.cullMode = VK_CULL_MODE_NONE;
		inout.multisampling = vkMultisamplingStateCreateInfo();
		inout.colorBlendAttachments.push_back(vkColorBlendAttachmentState());
		inout.depthStencil = vkDepthStencilCreateInfo(true,true,getEngineZTestFunc());
	}
	else
	{
		LOG_FATAL("Unkonw type! Need to process!");
	}
}

void engine::Material::constructMaterial(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,shaderCompiler::EShaderPass type)
{
	m_shaderCompiler = shaderCompiler;
	CHECK(m_materialShaderInfo);
	CHECK(m_shaderInfo);

	// NOTE: 此处填充第四个反射
	for(auto& pair : m_shaderInfo->effect->bindings)
	{
		if(pair.second.set == 3) // NOTE: 材质系统目前仅反射第4个材质符集
		{
			if(pair.second.type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				// NOTE: 增加纹理引用
				if(m_materialShaderInfo->textures.find(pair.first) == m_materialShaderInfo->textures.end())
				{
					m_materialShaderInfo->textures[pair.first] = { pair.second.binding,"" };
				}
				else if(m_materialShaderInfo->textures[pair.first].first != pair.second.binding) // 确保已有的binding和名字对上
				{
					m_materialShaderInfo->textures[pair.first].first = pair.second.binding;
				}
			}
			else
			{
				LOG_FATAL("Current no know other types!");
			}
		}
	}
}

// 根据 m_materialShaderInfo 填入的信息构建材质符集
void engine::Material::buildMeshDescriptorSet(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,const MeshPassLayout& pass_layout,bool bRebuildMaterial)
{
	// 修改了
	if(m_shaderInfo->shaderName != m_materialShaderInfo->shaderName || !m_shaderInfo->compiled || !m_shaderInfo->compiled_suceess)
	{
		m_shaderInfo = shaderCompiler->getMeshPassShaderInfo(
			m_materialShaderInfo->shaderName,
			shaderCompiler::EShaderPass::GBuffer
		);
		constructMaterial(shaderCompiler,shaderCompiler::EShaderPass::GBuffer);
	}

	m_depthShaderInfo = shaderCompiler->getMeshPassShaderInfo(s_shader_depth,shaderCompiler::EShaderPass::ShadowDepth);
	auto findAllRequireTexturesReady = [&]() -> bool
	{
		bool result = true;
		for(auto& pair : m_shaderInfo->effect->bindings)
		{
			if(pair.second.set == 3) // NOTE: 材质系统目前仅反射第4个材质符集
			{
				auto& bindingPair = m_materialShaderInfo->textures[pair.first];
				result = result && !TextureLibrary::get()->textureLoading(bindingPair.second);
			}
		}
		return result;
	};

	if(m_lastTimeUseTempTexture && findAllRequireTexturesReady())
	{
		m_gbufferDescriptorSet.bRebuild = true;
		LOG_INFO("Material's textures load ready and recreate descriptor!");
	}

	const bool bGbufferRebuild = bRebuildMaterial || m_gbufferDescriptorSet.bRebuild;
	// NOTE: 使用宽松的多线程写入限制，无论如何它们都会刷新至少一次并且刷新此时为有限次
	if(bGbufferRebuild) // #1. Gbuffer的描述符
	{
		// 释放之前的资源
		releaseGbufferResource();
		m_hasCustomDescriptor = false;

		// TODO: 根据反射信息构建pipeline而不是HardCode
		VulkanGraphicsPipelineFactory pipelineBuilder{ };
		fillPipelineBuilder(pipelineBuilder,shaderCompiler::EShaderPass::GBuffer);
		pipelineBuilder.shaderStages.clear();
		m_shaderInfo->effect->fillStages(pipelineBuilder.shaderStages);
		pipelineBuilder.pipelineLayout = m_shaderInfo->effect->builtLayout;
		m_gbufferPipeline = pipelineBuilder.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),pass_layout.gBufferPass);

		auto df =  VulkanRHI::get()->vkDescriptorFactoryBegin();
		m_lastTimeUseTempTexture = false;
		for(auto& pair : m_shaderInfo->effect->bindings)
		{
			if(pair.second.set == 3) // NOTE: 材质系统目前仅反射第4个材质符集
			{
				auto& bindingPair = m_materialShaderInfo->textures[pair.first];

				VkDescriptorImageInfo imageBufferInfo {};
				auto textureRequirePair = TextureLibrary::get()->getCombineTextureByName(bindingPair.second);
				auto& combineTexture = textureRequirePair.second;
				imageBufferInfo.sampler = combineTexture.sampler;
				imageBufferInfo.imageView = combineTexture.texture->getImageView();
				imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				df.bindImage(
					bindingPair.first, 
					&imageBufferInfo,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

				m_hasCustomDescriptor = true;
				m_lastTimeUseTempTexture = m_lastTimeUseTempTexture || (textureRequirePair.first == ERequestTextureResult::Loading);
			}
		}

		df.build(&m_gbufferDescriptorSet.set.set);
		m_gbufferDescriptorSet.bRebuild = false;
	}

	if(bGbufferRebuild && pass_layout.shadowDepth != VK_NULL_HANDLE) // #2. Shadow Depth的描述符
	{
		// 释放之前的资源
		releaseDepthResource();

		VulkanGraphicsPipelineFactory pipelineBuilder{ };
		fillPipelineBuilder(pipelineBuilder,shaderCompiler::EShaderPass::ShadowDepth);
		pipelineBuilder.shaderStages.clear();

		auto depthShaderInfo = shaderCompiler->getMeshPassShaderInfo(s_shader_depth,shaderCompiler::EShaderPass::ShadowDepth);
		depthShaderInfo->effect->fillStages(pipelineBuilder.shaderStages);
		pipelineBuilder.pipelineLayout = depthShaderInfo->effect->builtLayout;
		m_depthPipeline = pipelineBuilder.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),pass_layout.shadowDepth);

		auto df =  VulkanRHI::get()->vkDescriptorFactoryBegin();

		for(auto& pair : depthShaderInfo->effect->bindings)
		{
			if(pair.second.set == 3) // NOTE: 使用和普通Gbuffer相同的描述符反射
									 //       Gbuffer shader在哪里采样alpha深度就用哪个采样alpha
			{
				auto& bindingPair = m_materialShaderInfo->textures[pair.first];

				VkDescriptorImageInfo imageBufferInfo {};
				auto textureRequirePair = TextureLibrary::get()->getCombineTextureByName(bindingPair.second);
				auto& combineTexture = textureRequirePair.second;
				imageBufferInfo.sampler = combineTexture.sampler;
				imageBufferInfo.imageView = combineTexture.texture->getImageView();
				imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				df.bindImage(
					bindingPair.first, 
					&imageBufferInfo,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			}
		}

		df.build(&m_depthDesctiptorSet.set.set);
		m_depthDesctiptorSet.bRebuild = false;
	} 
}

void engine::Material::bindGbuffer(VkCommandBuffer cmd)
{
	if(m_hasCustomDescriptor)
	{
		vkCmdBindDescriptorSets(
			cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
			getShaderInfo()->effect->builtLayout,
			3, // PassSet #3
			1,
			&m_gbufferDescriptorSet.set.set,0,nullptr
		);
	}
}

void engine::Material::bindDepth(VkCommandBuffer cmd)
{
	if(m_hasCustomDescriptor)
	{
		vkCmdBindDescriptorSets(
			cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_depthShaderInfo->effect->builtLayout,
			3, // PassSet #3
			1,
			&m_depthDesctiptorSet.set.set,0,nullptr
		);
	}
}

// NOTE: 构建函数填入名字和PassType
engine::MaterialShaderInfo::MaterialShaderInfo(const std::string& name,shaderCompiler::EShaderPass type)
{
	shaderName = name;
	passType = type;
}

const std::string materialSuffix = ".material";
bool engine::MaterialShaderInfo::createEmptyMaterialAsset(const std::string& path)
{
	if(std::filesystem::exists(path + materialSuffix))
	{
		return createEmptyMaterialAsset(path + "(1)");
	}

	const std::string finalName = path + materialSuffix;

	MaterialShaderInfo materialInfo{};
	materialInfo.passType = shaderCompiler::EShaderPass::GBuffer;
	materialInfo.shaderName = shaderCompiler::fallbackShaderName::gbuffer;

	std::ofstream ofs(finalName);
	cereal::JSONOutputArchive archive(ofs);
	archive(materialInfo);

	return true;
}

void engine::MaterialLibrary::init(Ref<shaderCompiler::ShaderCompiler> shaderCompiler)
{
	release();
	m_shaderCompiler = shaderCompiler;

	// NOTE: 构建回滚材质
	{
		Material* fallbackGbuffer = new Material();
		auto materialInfo = std::make_shared<MaterialShaderInfo>();
		materialInfo->shaderName = shaderCompiler::fallbackShaderName::gbuffer;

		MaterialInfoCache::s_cache->cacheInfos[shaderCompiler::fallbackShaderName::gbuffer] = materialInfo;

		fallbackGbuffer->m_materialShaderInfo = materialInfo;
		fallbackGbuffer->m_shaderInfo = shaderCompiler->getMeshPassShaderInfo(
			shaderCompiler::fallbackShaderName::gbuffer,
			shaderCompiler::EShaderPass::GBuffer
		);

		fallbackGbuffer->constructMaterial(shaderCompiler,shaderCompiler::EShaderPass::GBuffer);
		m_materialContainer[shaderCompiler::fallbackShaderName::gbuffer] = fallbackGbuffer;
	}
	{
		Material* fallbackDepth = new Material();
		auto materialInfo = std::make_shared<MaterialShaderInfo>();
		materialInfo->shaderName = shaderCompiler::fallbackShaderName::depth;
		MaterialInfoCache::s_cache->cacheInfos[shaderCompiler::fallbackShaderName::depth] = materialInfo;

		fallbackDepth->m_materialShaderInfo = materialInfo;
		fallbackDepth->m_shaderInfo = shaderCompiler->getMeshPassShaderInfo(
			shaderCompiler::fallbackShaderName::depth,
			shaderCompiler::EShaderPass::ShadowDepth
		);
		fallbackDepth->constructMaterial(shaderCompiler,shaderCompiler::EShaderPass::ShadowDepth);
		m_materialContainer[shaderCompiler::fallbackShaderName::depth] = fallbackDepth;
	}
	
	bInit = true;
}

Ref<Material> engine::MaterialLibrary::registerMaterial(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,const std::string& materialInfoName)
{
	if(!bInit) return nullptr;

	if(m_materialContainer.find(materialInfoName) != m_materialContainer.end())
	{
		return m_materialContainer[materialInfoName];
	}

	Material* meshMaterial = new Material();

	meshMaterial->m_materialShaderInfo = MaterialInfoCache::s_cache->getMaterialInfoCache(materialInfoName);
	meshMaterial->m_shaderInfo = shaderCompiler->getMeshPassShaderInfo(
		meshMaterial->m_materialShaderInfo->shaderName,
		shaderCompiler::EShaderPass::GBuffer
	);

	meshMaterial->constructMaterial(shaderCompiler,shaderCompiler::EShaderPass::GBuffer);
	m_materialContainer[materialInfoName] = meshMaterial;
	return m_materialContainer[materialInfoName];
}

void engine::MaterialLibrary::release()
{
	for(auto& pair : m_materialContainer)
	{
		pair.second->release();
		delete pair.second;
	}

	m_materialContainer.clear();
	bInit = false;
}

Ref<Material> engine::MaterialLibrary::getFallback(const MeshPassLayout& layout,shaderCompiler::EShaderPass passType)
{
	switch(passType)
	{
	case engine::shaderCompiler::EShaderPass::GBuffer:
	{
		auto* material = m_materialContainer[shaderCompiler::fallbackShaderName::gbuffer];
		material->buildMeshDescriptorSet(m_shaderCompiler,layout,false);
		return material;
	}
	case engine::shaderCompiler::EShaderPass::ShadowDepth:
	{
		auto* material = m_materialContainer[shaderCompiler::fallbackShaderName::depth];
		material->buildMeshDescriptorSet(m_shaderCompiler,layout,false);
		return material;
	}
	default:
	{
		LOG_FATAL("Unhandle mesh pass type!");
	}
	}
}


std::shared_ptr<MaterialShaderInfo> engine::MaterialInfoCache::getMaterialInfoCache(const std::string& name)
{
	if(cacheInfos.find(name) != cacheInfos.end())
	{
		return cacheInfos[name];
	}
	else
	{
		auto newInfo = std::make_shared<MaterialShaderInfo>();
		cacheInfos[name] = newInfo;

		// 材质我们需要序列化到文件中
		std::ifstream os(name);
		cereal::JSONInputArchive iarchive(os);
		iarchive(*newInfo);

		return newInfo;
	}
}
