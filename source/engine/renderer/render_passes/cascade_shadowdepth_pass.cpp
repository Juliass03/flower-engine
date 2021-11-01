#include "cascade_shadowdepth_pass.h"
#include "../../launch/launch_engine_loop.h"
#include "../mesh.h"
#include "../material.h"
#include "../texture.h"
#include "../renderer.h"
#include "../compute_passes/gpu_culling.h"

using namespace engine;

// Using fixed split partiotion size to avoid shadow flicking.
const float g_cascadePartitionsZeroToOne4[4] = { 6.7f,  13.3f, 26.7f, 53.3f };
const float g_cascadePartitionsMax = 100.0f;

static AutoCVarFloat cVarShadowDrawDistance(
	"r.Shadow.DrawDistance",
	"Cascade shadow draw distance.",
	"Shadow",
	220.0f,
	CVarFlags::ReadAndWrite
);

static AutoCVarFloat cVarShadowDepthHardwareSlope(
	"r.Shadow.DepthHardwareSlope",
	"Cascade shadow depth hardware slope.",
	"Shadow",
	2.15f,
	CVarFlags::ReadAndWrite
);

static AutoCVarFloat cVarShadowDepthBias(
	"r.Shadow.DepthHardwareBias",
	"Cascade shadow depth hardware bias.",
	"Shadow",
	1.25f,
	CVarFlags::ReadAndWrite
);

static AutoCVarInt32 cVarSingleShadowMapSize(
	"r.Shadow.ShadowMapSize",
	"Single shadow map size.",
	"Shadow",
	2048,
	CVarFlags::ReadOnly | CVarFlags::InitOnce 
);

static AutoCVarInt32 cVarShadowEnableDepthClamp(
	"r.Shadow.DepthClamp",
	"Enable depth clamp on shadow.",
	"Shadow",
	1,
	CVarFlags::ReadAndWrite
);

void Cascade::SetupCascades(
	std::vector<Cascade>& inout,
	float nearClip,
	const glm::mat4& cameraViewProj,
	const glm::vec3& inLightDir,
	RenderScene* inScene)
{
	inout.clear();

	uint32 cascadeNum = CASCADE_MAX_COUNT;
	inout.resize(cascadeNum);

	// 相机视锥裁剪距离
	float clipRange = cVarShadowDrawDistance.get();
	float minZ = nearClip;
	float maxZ = nearClip + clipRange;
	float ratio = maxZ/minZ;

	// 齐次空间下的相机角点
	glm::vec3 frustumCorners[8] =
	{
		glm::vec3(-1.0f,  1.0f, -1.0f),
		glm::vec3(1.0f,  1.0f, -1.0f),
		glm::vec3(1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f,  1.0f,  1.0f),
		glm::vec3(1.0f,  1.0f,  1.0f),
		glm::vec3(1.0f, -1.0f,  1.0f),
		glm::vec3(-1.0f, -1.0f,  1.0f),
	};

	// 转化为世界空间下
	glm::mat4 invCam = glm::inverse(cameraViewProj);
	float Zmax = std::numeric_limits<float>::min();
	float Zmin = std::numeric_limits<float>::max();
	for(uint32 i = 0; i<8; i++)
	{
		glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i],1.0f);
		frustumCorners[i] = invCorner/invCorner.w; // 齐次除法
		Zmax = glm::max(frustumCorners[i].z,Zmax);
		Zmin = glm::min(frustumCorners[i].z,Zmin);
	}

	// 遍历每个cascade来计算它的正交投影矩阵
	float lastSplitDist = 0.0f;
	for(uint32 cascadeIndex = 0; cascadeIndex<cascadeNum; cascadeIndex++)
	{
		float frustumIntervalSplit;

		frustumIntervalSplit = g_cascadePartitionsZeroToOne4[cascadeIndex];
		frustumIntervalSplit /= g_cascadePartitionsMax;
		frustumIntervalSplit *= clipRange;

		// 计算切割距离
		glm::vec3 intervalCorners[8] = {};

		// 计算子视锥角点
		for(uint32 i = 0; i < 4; i++)
		{
			// Dist is a stable value.
			glm::vec3 dist = glm::normalize(frustumCorners[i+4] - frustumCorners[i]);
			intervalCorners[i+4] = frustumCorners[i] + (dist * frustumIntervalSplit);
			intervalCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// 计算视锥包围球
		glm::vec3 intervalCenter = glm::vec3(0.0f);
		for(uint32 i = 0; i < 8; i++)
		{
			intervalCenter += intervalCorners[i];
		}
		intervalCenter /= 8.0f;

		// 计算视锥半径
		float radius = 0.0f;
		for(uint32 i = 0; i < 8; i++)
		{
			float distance = glm::length(intervalCorners[i]-intervalCenter);
			radius = glm::max(radius,distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = glm::normalize(inLightDir);
		glm::mat4 lightViewMatrix = glm::lookAt(
			intervalCenter - lightDir * -minExtents.z,
			intervalCenter,
			glm::vec3(0.0f,1.0f,0.0f) // stable 
		);

		glm::mat4 lightOrthoMatrix;

		const bool bReverseZ = reverseZOpen();
		float nearZ = bReverseZ ? maxExtents.z - minExtents.z : 0.0f;
		float farZ  = bReverseZ ? 0.0f : maxExtents.z - minExtents.z;

		lightOrthoMatrix = glm::ortho(
			minExtents.x,
			maxExtents.x,
			minExtents.y,
			maxExtents.y,
			nearZ,
			farZ
		);

		inout[cascadeIndex].viewProj = lightOrthoMatrix * lightViewMatrix;

		// NOTE: Shadow Map Texel移动对齐解决Flicking
		glm::vec4 shadowOrigin = glm::vec4(0.0f,0.0f,0.0f,1.0f);

		shadowOrigin = inout[cascadeIndex].viewProj * shadowOrigin;
		shadowOrigin = shadowOrigin * ((float)cVarSingleShadowMapSize.get() * 0.5f);

		glm::vec4 roundOrign = glm::round(shadowOrigin);
		glm::vec4 roundOffset = roundOrign - shadowOrigin;

		roundOffset = roundOffset * 2.0f / (float)cVarSingleShadowMapSize.get();

		lightOrthoMatrix[3][0] = lightOrthoMatrix[3][0] + roundOffset.x;
		lightOrthoMatrix[3][1] = lightOrthoMatrix[3][1] + roundOffset.y;

		inout[cascadeIndex].viewProj = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = frustumIntervalSplit;
	}
}

void engine::ShadowDepthPass::initInner()
{
    createRenderpass();
    createFramebuffers();
	createPipeline();

    m_deletionQueue.push([&]()
    {
        destroyRenderpass();
        destroyFramebuffers();
		destroyPipeline();
    });
}

void engine::ShadowDepthPass::beforeSceneTextureRecreate()
{
    destroyFramebuffers();
	destroyPipeline();
}

void engine::ShadowDepthPass::afterSceneTextureRecreate()
{
    createFramebuffers();
	createPipeline();
}

void engine::ShadowDepthPass::cascadeRecord(VkCommandBuffer& cmd,uint32 backBufferIndex,ECullIndex cullIndexType)
{
	uint32 cascadeIndex = cullingIndexToCasacdeIndex(cullIndexType);
	CHECK(cascadeIndex < 4);

    auto shadowTextureExtent = m_renderScene->getSceneTextures().getCascadeShadowDepthMapArray()->getExtent();
    VkExtent2D shadowTextureExtent2D{};
	shadowTextureExtent2D.width = shadowTextureExtent.width;
	shadowTextureExtent2D.height = shadowTextureExtent.height;

    VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(
		getRenderpass(),
		shadowTextureExtent2D,
		m_cascadeFramebuffers[backBufferIndex][cascadeIndex]
	);

    VkClearValue clearValue{};
    clearValue.depthStencil = { getEngineClearZFar(), 1 };
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    VkRect2D scissor{};
    scissor.extent = shadowTextureExtent2D;
    scissor.offset = {0,0};
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)shadowTextureExtent2D.width;
    viewport.height = (float)shadowTextureExtent2D.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

	if(m_renderScene->isSceneEmpty())
	{
		vkCmdBeginRenderPass(cmd,&rpInfo,VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetScissor(cmd,0,1,&scissor);
		vkCmdSetViewport(cmd,0,1,&viewport);
		vkCmdSetDepthBias(cmd,0,0,0);
		vkCmdEndRenderPass(cmd);
		return;
	}

	GPUCascadePushConstants gpuPushConstant = {};
	gpuPushConstant.cascadeIndex = cascadeIndex;

	VkDeviceSize asyncRange = (uint32)m_renderScene->m_cacheMeshObjectSSBOData.size() * sizeof(GPUDrawCallData);
	std::array<VkBufferMemoryBarrier,2> bufferBarriers{};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

	bufferBarriers[0].buffer = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].drawIndirectSSBO->GetVkBuffer();
	bufferBarriers[1].buffer = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].countBuffer->GetVkBuffer();
	
	bufferBarriers[0].size = asyncRange;
	bufferBarriers[1].size = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].countSize;
	
	bufferBarriers[0].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

	bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0,
		0,nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0,nullptr);

	vkCmdBeginRenderPass(cmd,&rpInfo,VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetScissor(cmd,0,1,&scissor);
	vkCmdSetViewport(cmd,0,1,&viewport);

	const float depthBiasSlope = cVarShadowDepthHardwareSlope.get();
	const float depthBiasFactor = cVarShadowDepthBias.get();
	vkCmdSetDepthBias(cmd,depthBiasFactor,0,depthBiasSlope);

	std::vector<VkDescriptorSet> meshPassSets = {
		  m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set
		, TextureLibrary::get()->getBindlessTextureDescriptorSet()
		, m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSets.set
		, m_renderer->getRenderScene().m_meshMaterialSSBO->descriptorSets.set
		, m_renderer->getRenderScene().m_drawIndirectSSBOShadowDepths[cascadeIndex].descriptorSets.set
	};

	vkCmdBindDescriptorSets(
		cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_pipelineLayouts[backBufferIndex],
		0,
		(uint32)meshPassSets.size(),
		meshPassSets.data(),
		0,
		nullptr
	);

	vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUCascadePushConstants), &gpuPushConstant);

	vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_pipelines[backBufferIndex]);
	vkCmdDrawIndexedIndirectCount(
		cmd,
		m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].drawIndirectSSBO->GetVkBuffer(),
		0,
		m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].countBuffer->GetVkBuffer(),
		0,
		(uint32)m_renderScene->m_cacheMeshObjectSSBOData.size(),
		sizeof(GPUDrawCallData)
	);

	vkCmdEndRenderPass(cmd);

	bufferBarriers[0].srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[1].srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	bufferBarriers[0].buffer = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].drawIndirectSSBO->GetVkBuffer();
	bufferBarriers[1].buffer = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].countBuffer->GetVkBuffer();

	bufferBarriers[0].size = asyncRange;
	bufferBarriers[1].size = m_renderScene->m_drawIndirectSSBOShadowDepths[cascadeIndex].countSize;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0,nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0,nullptr);
}

void engine::ShadowDepthPass::createRenderpass()
{
    VkAttachmentDescription attachmentDesc{};
    attachmentDesc.format = m_renderScene->getSceneTextures().getCascadeShadowDepthMapArray()->getFormat();
    attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 0;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachmentDesc;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = (uint32)dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();

    m_renderpass = VulkanRHI::get()->createRenderpass(renderPassInfo);
}

void engine::ShadowDepthPass::destroyRenderpass()
{
    VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void engine::ShadowDepthPass::createFramebuffers()
{
    const uint32 swapchain_imagecount = (uint32)VulkanRHI::get()->getSwapchainImages().size();
	m_cascadeFramebuffers.resize(swapchain_imagecount);

    for(uint32 i = 0; i < swapchain_imagecount; i++)
    {
		auto& cascadeFramebuffers = m_cascadeFramebuffers[i];
		cascadeFramebuffers.resize(CASCADE_MAX_COUNT);
		for(uint32 j = 0; j < CASCADE_MAX_COUNT; j++) // 每个级联的framebuffer都有创建一个
		{
			VulkanFrameBufferFactory fbf {};
			fbf.setRenderpass(m_renderpass)
			   .addArrayAttachment(m_renderScene->getSceneTextures().getCascadeShadowDepthMapArray(),j);
			cascadeFramebuffers[j] = fbf.create(VulkanRHI::get()->getDevice());
		}
    }
}

void engine::ShadowDepthPass::destroyFramebuffers()
{
	for(auto& cascadeFramebuffers : m_cascadeFramebuffers)
	{
		for(auto& framebuffer : cascadeFramebuffers)
		{
			VulkanRHI::get()->destroyFramebuffer(framebuffer);
		}
	}

	m_cascadeFramebuffers.resize(0);
}

void engine::ShadowDepthPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(GPUCascadePushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		plci.pPushConstantRanges = &push_constant;
		plci.pushConstantRangeCount = 1; // NOTE: 我们有一个PushConstant

		std::vector<VkDescriptorSetLayout> setLayouts = {
			  m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout
			, TextureLibrary::get()->getBindlessTextureDescriptorSetLayout()
			, m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSetLayout.layout
			, m_renderer->getRenderScene().m_meshMaterialSSBO->descriptorSetLayout.layout
			, m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSetLayout.layout
		};
		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();
		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		VulkanGraphicsPipelineFactory gpf = {};
		auto packShader = m_shaderCompiler->getShader(s_shader_depth,shaderCompiler::EShaderPass::Depth);

		auto vertShader = packShader.vertex;
		auto fragShader = packShader.frag;

		gpf.shaderStages.clear();
		gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertShader));
		gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragShader));

		VulkanVertexInputDescription vvid = {};
		vvid.bindings   =  { VulkanVertexBuffer::getInputBinding(getStandardMeshAttributes()) };
		vvid.attributes = VulkanVertexBuffer::getInputAttribute(getStandardMeshAttributes());

		gpf.vertexInputDescription = vvid;
		gpf.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		gpf.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

		gpf.rasterizer.cullMode = VK_CULL_MODE_NONE; // TODO: Cull Front

		gpf.rasterizer.depthBiasEnable = VK_TRUE;
		gpf.rasterizer.depthClampEnable = cVarShadowEnableDepthClamp.get() == 1 ? VK_TRUE : VK_FALSE;
		gpf.multisampling = vkMultisamplingStateCreateInfo();
		gpf.depthStencil = vkDepthStencilCreateInfo(true,true,getEngineZTestFunc());
		gpf.pipelineLayout =  m_pipelineLayouts[index];

		auto shadowExtent = m_renderScene->getSceneTextures().getCascadeShadowDepthMapArray()->getExtent();
		VkExtent2D shadowExtent2D{};

		shadowExtent2D.width = shadowExtent.width;
		shadowExtent2D.height = shadowExtent.height;

		gpf.viewport.x = 0.0f;
		gpf.viewport.y = 0.0f;
		gpf.viewport.width =  (float)shadowExtent2D.width;
		gpf.viewport.height = (float)shadowExtent2D.height;
		gpf.viewport.minDepth = 0.0f;
		gpf.viewport.maxDepth = 1.0f;
		gpf.scissor.offset = { 0, 0 };
		gpf.scissor.extent = shadowExtent2D;

		m_pipelines[index] = gpf.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),m_renderpass);
	}

	bInitPipeline = true;
}

void engine::ShadowDepthPass::destroyPipeline()
{
	if(!bInitPipeline) return;

	for(uint32 index = 0; index < m_pipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
	}
	m_pipelines.resize(0);
	m_pipelineLayouts.resize(0);

	bInitPipeline = false;
}
