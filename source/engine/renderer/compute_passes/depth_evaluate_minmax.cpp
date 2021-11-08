#include "depth_evaluate_minmax.h"
#include "../renderer.h"
using namespace engine;

#define WORK_TILE_SIZE 16

struct GpuDepthEvaluateMinMaxPushConstant
{
	glm::vec2 imageSize;
};

void engine::GpuDepthEvaluateMinMaxPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::GpuDepthEvaluateMinMaxPass::beforeSceneTextureRecreate()
{
	destroyPipeline();
}

void engine::GpuDepthEvaluateMinMaxPass::afterSceneTextureRecreate()
{
	createPipeline();
}

void engine::GpuDepthEvaluateMinMaxPass::record(VkCommandBuffer& cmd,uint32 backBufferIndex)
{
	if(m_renderScene->isSceneEmpty())
	{
		return;
	}

	// Need gbuffer depth.
	// Need depth min max buffer. 
	std::array<VkImageMemoryBarrier,1> imageBarriers {};
	imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

	imageBarriers[0].image = m_renderScene->getSceneTextures().getDepthStencil()->getImage();
	imageBarriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[0].subresourceRange.levelCount = 1;
	imageBarriers[0].subresourceRange.layerCount = 1;
	imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
	imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL; 

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		(uint32)imageBarriers.size(), imageBarriers.data()
	);


	std::array<VkBufferMemoryBarrier,1> bufferBarriers {};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

	GpuDepthEvaluateMinMaxPushConstant gpuPushConstant = {};
	auto depthImageExtent = m_renderScene->getSceneTextures().getDepthStencil()->getExtent();
	gpuPushConstant.imageSize.x = float(depthImageExtent.width);
	gpuPushConstant.imageSize.y = float(depthImageExtent.height);

	VkDeviceSize asyncRange = m_renderScene->m_evaluateDepthMinMax.size;
	bufferBarriers[0].buffer = m_renderScene->m_evaluateDepthMinMax.buffer->GetVkBuffer();
	bufferBarriers[0].size = asyncRange;
	bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0,nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0,nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

	vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], 
	VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GpuDepthEvaluateMinMaxPushConstant), &gpuPushConstant);

	std::vector<VkDescriptorSet> compPassSets = {
		  m_descriptorSets[backBufferIndex].set
		, m_renderer->getRenderScene().m_evaluateDepthMinMax.descriptorSets.set
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

	vkCmdDispatch(cmd, 
		getGroupCount(depthImageExtent.width, WORK_TILE_SIZE), 
		getGroupCount(depthImageExtent.height,WORK_TILE_SIZE), 
		1
	);


	imageBarriers[0].image = m_renderScene->getSceneTextures().getDepthStencil()->getImage();
	imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		(uint32)imageBarriers.size(), imageBarriers.data()
	);

	bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferBarriers[0].buffer = m_renderScene->m_evaluateDepthMinMax.buffer->GetVkBuffer();
	bufferBarriers[0].size = asyncRange;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0, nullptr);
}

void engine::GpuDepthEvaluateMinMaxPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
	
	m_descriptorSets.resize(backBufferCount);
	m_descriptorSetLayouts.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		VkDescriptorImageInfo depthStencilImage = {};
		depthStencilImage.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		depthStencilImage.imageView = ((DepthStencilImage*)(m_renderScene->getSceneTextures().getDepthStencil()))->getDepthOnlyImageView();
		depthStencilImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

		m_renderer->vkDynamicDescriptorFactoryBegin(index)
			.bindImage(0,&depthStencilImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(m_descriptorSets[index],m_descriptorSetLayouts[index]);
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(GpuDepthEvaluateMinMaxPushConstant);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		plci.pPushConstantRanges = &push_constant;
		plci.pushConstantRangeCount = 1; // NOTE: 我们有一个PushConstant

		
		std::vector<VkDescriptorSetLayout> setLayouts = {
			  m_descriptorSetLayouts[index].layout
			, m_renderer->getRenderScene().m_evaluateDepthMinMax.descriptorSetLayout.layout
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/evaluateDepthMinMax.comp.spv");
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

	bInitPipeline = true;
}

void engine::GpuDepthEvaluateMinMaxPass::destroyPipeline()
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
