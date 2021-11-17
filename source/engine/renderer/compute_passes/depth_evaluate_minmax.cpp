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

void engine::GpuDepthEvaluateMinMaxPass::record(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);

	GpuDepthEvaluateMinMaxPushConstant gpuPushConstant = {};
	auto depthImageExtent = m_renderScene->getSceneTextures().getDepthStencil()->getExtent();
	gpuPushConstant.imageSize.x = float(depthImageExtent.width);
	gpuPushConstant.imageSize.y = float(depthImageExtent.height);


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

	commandBufEnd(backBufferIndex);
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
		depthStencilImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthStencilImage.imageView = m_renderScene->getSceneTextures().getDepthStencil()->getImageView();;
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

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/evaluateDepthMinMax.comp.spv",true);
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
