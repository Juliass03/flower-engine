#include "downsample.h"
#include "../renderer.h"

using namespace engine;

static AutoCVarFloat cVarBloomThreshold(
	"r.Bloom.Threshold",
	"Bloom hard threshold",
	"Bloom",
	0.45f,
	CVarFlags::ReadAndWrite
);

static AutoCVarFloat cVarBloomSoftThreshold(
	"r.Bloom.ThresholdSoft",
	"Bloom solf threshold",
	"Bloom",
	0.5f,
	CVarFlags::ReadAndWrite
);

struct PushConstant
{
	glm::vec4 filter;
	glm::vec2 invSize;
	glm::uvec2 size;
	int32_t mipLevel;
};

void engine::DownSamplePass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::DownSamplePass::beforeSceneTextureRecreate()
{
	destroyPipeline();
}

void engine::DownSamplePass::afterSceneTextureRecreate()
{
	createPipeline();
}

void engine::DownSamplePass::destroyPipeline()
{
	if(!bInitPipeline) return;

	for(uint32 index = 0; index<m_pipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
	}
	m_pipelines.resize(0);
	m_pipelineLayouts.resize(0);
	bInitPipeline = false;
}



void engine::DownSamplePass::record(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);

	vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,m_pipelines[backBufferIndex]);

	{
		std::array<VkImageMemoryBarrier,1> imageBarriers{};
		imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[0].image = m_renderScene->getSceneTextures().getDownSampleChain()->getImage();
		imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageBarriers[0].subresourceRange.baseMipLevel = 0; // We barrier mip level i
		imageBarriers[0].subresourceRange.levelCount = g_downsampleCount;
		imageBarriers[0].subresourceRange.layerCount = 1;
		imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0,nullptr,
			0,nullptr,
			(uint32)imageBarriers.size(),imageBarriers.data()
		);
	}

	PushConstant pushData {};

	float knee = cVarBloomThreshold.get() * cVarBloomSoftThreshold.get();

	pushData.filter.x =  cVarBloomThreshold.get();
	pushData.filter.y = pushData.filter.x - knee;
	pushData.filter.z = 2.0f * knee;
	pushData.filter.w = 0.25f / (knee + 0.00001f);
	
	uint32 workingWidth = m_renderScene->getSceneTextures().getHDRSceneColor()->getInfo().extent.width;
	uint32 workingHeight = m_renderScene->getSceneTextures().getHDRSceneColor()->getInfo().extent.height;

	for(uint32 i = 0; i < g_downsampleCount; i++)
	{
		pushData.mipLevel = i;

		workingWidth  = std::max(1u, workingWidth  / 2);
		workingHeight = std::max(1u, workingHeight / 2);

		pushData.invSize = glm::vec2(
			1.0f / float(workingWidth),
			1.0f / float(workingHeight)
		);
		pushData.size = glm::uvec2(
			workingWidth,
			workingHeight
		);

		vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], VK_SHADER_STAGE_COMPUTE_BIT, 
			0, sizeof(PushConstant), &pushData);

		std::vector<VkDescriptorSet> compPassSets = {
			m_descriptorSets[backBufferIndex * g_downsampleCount + i].set,
			m_inputDescritorSets[backBufferIndex * g_downsampleCount + i].set
		};

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			m_pipelineLayouts[backBufferIndex],
			0,
			(uint32)compPassSets.size(),
			compPassSets.data(),
			0,
			nullptr
		);

		vkCmdDispatch(cmd, getGroupCount(workingWidth , 16),getGroupCount(workingHeight, 16), 1);
	

		std::array<VkImageMemoryBarrier,1> imageBarriers{};
		imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[0].image = m_renderScene->getSceneTextures().getDownSampleChain()->getImage();
		imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarriers[0].subresourceRange.baseMipLevel = i; // We barrier mip level i
		imageBarriers[0].subresourceRange.levelCount = 1;
		imageBarriers[0].subresourceRange.layerCount = 1;
		imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0,nullptr,
			0,nullptr,
			(uint32)imageBarriers.size(),imageBarriers.data()
		);
	}

	{
		std::array<VkImageMemoryBarrier,1> imageBarriers{};
		imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[0].image = m_renderScene->getSceneTextures().getDownSampleChain()->getImage();
		imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarriers[0].subresourceRange.baseMipLevel = 0; // We barrier mip level i
		imageBarriers[0].subresourceRange.levelCount = g_downsampleCount;
		imageBarriers[0].subresourceRange.layerCount = 1;
		imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0,nullptr,
			0,nullptr,
			(uint32)imageBarriers.size(),imageBarriers.data()
		);
	}

	

	commandBufEnd(backBufferIndex);
}

void engine::DownSamplePass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
	RenderTexture* downsampleChain = static_cast<RenderTexture*>(
		m_renderScene->getSceneTextures().getDownSampleChain()
	);

	m_descriptorSets.resize(backBufferCount * g_downsampleCount);
	m_descriptorSetLayouts.resize(backBufferCount);

	m_inputDescritorSets.resize(backBufferCount * g_downsampleCount);
	m_inputDescriptorSetLayouts.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		// For output
		uint32 ouputIndex = index * g_downsampleCount;
		for(uint32_t level = 0; level < downsampleChain->getInfo().mipLevels; ++level)
		{
			VkDescriptorImageInfo outImage = {};
			outImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outImage.imageView = downsampleChain->getMipmapView(level);
			outImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();

			if(level==0)
			{
				m_renderer->vkDynamicDescriptorFactoryBegin(index)
					.bindImage(
						0,
						&outImage,
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						VK_SHADER_STAGE_COMPUTE_BIT)
					.build(m_descriptorSets[ouputIndex], m_descriptorSetLayouts[index]);
			}
			else
			{
				m_renderer->vkDynamicDescriptorFactoryBegin(index)
					.bindImage(
						0,
						&outImage,
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						VK_SHADER_STAGE_COMPUTE_BIT)
					.build(m_descriptorSets[ouputIndex]);
			}
			ouputIndex ++;
		}

		// For input
		uint32 inputIndex = index * g_downsampleCount;

		VkDescriptorImageInfo hdrImage = {};
		hdrImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrImage.imageView = m_renderScene->getSceneTextures().getHDRSceneColor()->getImageView();
		hdrImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();
		m_renderer->vkDynamicDescriptorFactoryBegin(index)
			.bindImage(
				0,
				&hdrImage,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_COMPUTE_BIT)
			.build(m_inputDescritorSets[inputIndex],m_inputDescriptorSetLayouts[index]);

		
		for(uint32_t level = 0; level < downsampleChain->getInfo().mipLevels - 1; ++level)
		{
			inputIndex ++;

			VkDescriptorImageInfo inputChainImage = {};
			inputChainImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			inputChainImage.imageView = downsampleChain->getMipmapView(level);
			inputChainImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();

			m_renderer->vkDynamicDescriptorFactoryBegin(index)
				.bindImage(
					0,
					&inputChainImage,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_SHADER_STAGE_COMPUTE_BIT)
				.build(m_inputDescritorSets[inputIndex]);
		}
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();
		plci.pushConstantRangeCount = 1;

		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(PushConstant);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		plci.pPushConstantRanges = &push_constant;

		std::vector<VkDescriptorSetLayout> setLayouts = {
			m_descriptorSetLayouts[index].layout,
			m_inputDescriptorSetLayouts[index].layout
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		const VkSpecializationMapEntry specializationMap = { 0, 0, sizeof(uint32_t)};
		const uint32_t specializationData[] = { g_downsampleCount };
		const VkSpecializationInfo specializationInfo = { 1, &specializationMap, sizeof(specializationData), specializationData};


		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/downsample.comp.spv", true);
		VkPipelineShaderStageCreateInfo shaderStageCI{};
		shaderStageCI.module = shaderModule->GetModule();
		shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCI.pName = "main";
		shaderStageCI.pSpecializationInfo = &specializationInfo;

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_pipelineLayouts[index];
		computePipelineCreateInfo.flags = 0;
		computePipelineCreateInfo.stage = shaderStageCI;
		vkCheck(vkCreateComputePipelines(VulkanRHI::get()->getDevice(),nullptr,1,&computePipelineCreateInfo,nullptr,&m_pipelines[index]));
	}

	bInitPipeline = true;
}
