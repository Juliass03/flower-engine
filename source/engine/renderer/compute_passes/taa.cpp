#include "taa.h"
#include "../renderer.h"

using namespace engine;

void engine::TAAPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::TAAPass::beforeSceneTextureRecreate()
{
	destroyPipeline();
}

void engine::TAAPass::afterSceneTextureRecreate()
{
	createPipeline();
}

void engine::TAAPass::destroyPipeline()
{
	if(!bInitPipeline) return;

	for(uint32 index = 0; index<m_pipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
	}
	m_pipelines.resize(0);
	m_pipelineLayouts.resize(0);

	// sharpen
	for(uint32 index = 0; index < m_pipelinesSharpen.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelinesSharpen[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayoutsSharpen[index],nullptr);
	}
	m_pipelinesSharpen.resize(0);
	m_pipelineLayoutsSharpen.resize(0);

	bInitPipeline = false;
}



void engine::TAAPass::record(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

	std::vector<VkDescriptorSet> compPassSets = {
		m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set,
		m_descriptorSets[backBufferIndex].set
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

	m_renderScene->getSceneTextures().getTAA()->transitionLayout(cmd,VK_IMAGE_LAYOUT_GENERAL);
	m_renderScene->getSceneTextures().getHistory()->transitionLayout(cmd,VK_IMAGE_LAYOUT_GENERAL);

	auto extent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();
	vkCmdDispatch(cmd, getGroupCount(extent.width , 8),getGroupCount(extent.height, 8), 1);

	std::array<VkImageMemoryBarrier,2> imageBarriers {};
	imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarriers[0].image = m_renderScene->getSceneTextures().getTAA()->getImage();
	imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[0].subresourceRange.levelCount = 1;
	imageBarriers[0].subresourceRange.layerCount = 1;
	imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL; 

	imageBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarriers[1].image = m_renderScene->getSceneTextures().getHistory()->getImage();
	imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[1].subresourceRange.levelCount = 1;
	imageBarriers[1].subresourceRange.layerCount = 1;
	imageBarriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL; 
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		(uint32)imageBarriers.size(), imageBarriers.data()
	);

	m_renderScene->getSceneTextures().getHDRSceneColor()->transitionLayout(cmd,VK_IMAGE_LAYOUT_GENERAL);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelinesSharpen[backBufferIndex]);

	std::vector<VkDescriptorSet> compPassSetsSharpen = {
		m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set,
		m_descriptorSetsSharpen[backBufferIndex].set
	};

	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE, 
		m_pipelineLayoutsSharpen[backBufferIndex], 
		0, 
		(uint32)compPassSetsSharpen.size(), 
		compPassSetsSharpen.data(), 
		0, 
		nullptr
	);
	vkCmdDispatch(cmd, getGroupCount(extent.width , 8),getGroupCount(extent.height, 8), 1);
	m_renderScene->getSceneTextures().getHDRSceneColor()->transitionLayout(cmd,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	commandBufEnd(backBufferIndex);
}

void engine::TAAPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_descriptorSets.resize(backBufferCount);
	m_descriptorSetLayouts.resize(backBufferCount);
	m_descriptorSetsSharpen.resize(backBufferCount);
	m_descriptorSetLayoutsSharpen.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		// TAA Main
		{
			VkDescriptorImageInfo outTAAImage = {};
			outTAAImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outTAAImage.imageView = m_renderScene->getSceneTextures().getTAA()->getImageView();;
			outTAAImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

			VkDescriptorImageInfo depthImage = {};
			depthImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			depthImage.imageView = m_renderScene->getSceneTextures().getDepthStencil()->getImageView();
			depthImage.sampler = VulkanRHI::get()->getLinearClampSampler();

			VkDescriptorImageInfo historyImage = {};
			historyImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			historyImage.imageView = m_renderScene->getSceneTextures().getHistory()->getImageView();
			historyImage.sampler = VulkanRHI::get()->getLinearClampSampler();

			VkDescriptorImageInfo velocityImage = {};
			velocityImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			velocityImage.imageView = m_renderScene->getSceneTextures().getVelocity()->getImageView();
			velocityImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

			VkDescriptorImageInfo hdrImage = {};
			hdrImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			hdrImage.imageView = m_renderScene->getSceneTextures().getHDRSceneColor()->getImageView();
			hdrImage.sampler = VulkanRHI::get()->getLinearClampSampler();

			m_renderer->vkDynamicDescriptorFactoryBegin(index)
				.bindImage(0,&outTAAImage,   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(1,&depthImage,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(2,&historyImage,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(3,&velocityImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(4,&hdrImage,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(m_descriptorSets[index],m_descriptorSetLayouts[index]);
		}
		

		// TAA Sharpen
		{
			VkDescriptorImageInfo inTAAImage = {};
			inTAAImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			inTAAImage.imageView = m_renderScene->getSceneTextures().getTAA()->getImageView();;
			inTAAImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

			VkDescriptorImageInfo outHdrImage = {};
			outHdrImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outHdrImage.imageView = m_renderScene->getSceneTextures().getHDRSceneColor()->getImageView();;
			outHdrImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

			VkDescriptorImageInfo outHistoryImage = {};
			outHistoryImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outHistoryImage.imageView = m_renderScene->getSceneTextures().getHistory()->getImageView();;
			outHistoryImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

			m_renderer->vkDynamicDescriptorFactoryBegin(index)
				.bindImage(0,&outHdrImage,   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(1,&outHistoryImage,    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
				.bindImage(2,&inTAAImage,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(m_descriptorSetsSharpen[index],m_descriptorSetLayoutsSharpen[index]);
		}
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	m_pipelinesSharpen.resize(backBufferCount);
	m_pipelineLayoutsSharpen.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		// TAA Main
		{
			VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();
			plci.pushConstantRangeCount = 0; 

			std::vector<VkDescriptorSetLayout> setLayouts = {
				m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout,
				m_descriptorSetLayouts[index].layout
			};

			plci.setLayoutCount = (uint32)setLayouts.size();
			plci.pSetLayouts = setLayouts.data();

			m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

			auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/taa.comp.spv",true);
			VkPipelineShaderStageCreateInfo shaderStageCI{};
			shaderStageCI.module = shaderModule->GetModule();
			shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shaderStageCI.pName = "main";

			VkComputePipelineCreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			computePipelineCreateInfo.layout = m_pipelineLayouts[index];
			computePipelineCreateInfo.flags = 0;
			computePipelineCreateInfo.stage = shaderStageCI;
			vkCheck(vkCreateComputePipelines(VulkanRHI::get()->getDevice(),nullptr,1,&computePipelineCreateInfo,nullptr,&m_pipelines[index]));
		}
		
		// TAA Sharpen
		{
			VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();
			plci.pushConstantRangeCount = 0; 

			std::vector<VkDescriptorSetLayout> setLayouts = {
				m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout,
				m_descriptorSetLayoutsSharpen[index].layout
			};

			plci.setLayoutCount = (uint32)setLayouts.size();
			plci.pSetLayouts = setLayouts.data();
			m_pipelineLayoutsSharpen[index] = VulkanRHI::get()->createPipelineLayout(plci);

			auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/taa_sharpen.comp.spv",true);
			VkPipelineShaderStageCreateInfo shaderStageCI{};
			shaderStageCI.module = shaderModule->GetModule();
			shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shaderStageCI.pName = "main";

			VkComputePipelineCreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			computePipelineCreateInfo.layout = m_pipelineLayoutsSharpen[index];
			computePipelineCreateInfo.flags = 0;
			computePipelineCreateInfo.stage = shaderStageCI;
			vkCheck(vkCreateComputePipelines(VulkanRHI::get()->getDevice(),nullptr,1,&computePipelineCreateInfo,nullptr,&m_pipelinesSharpen[index]));
		}
	}

	bInitPipeline = true;
}
