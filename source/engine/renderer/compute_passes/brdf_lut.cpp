#include "brdf_lut.h"
#include "../renderer.h"

using namespace engine;

void engine::GpuBRDFLutPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::GpuBRDFLutPass::record(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

	std::vector<VkDescriptorSet> compPassSets = {
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

	vkCmdDispatch(cmd, SceneTextures::getBRDFLutDim() / 16, SceneTextures::getBRDFLutDim() / 16, 1);
	commandBufEnd(backBufferIndex);
}

void engine::GpuBRDFLutPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_descriptorSets.resize(backBufferCount);
	m_descriptorSetLayouts.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		VkDescriptorImageInfo lutImage = {};
		lutImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		lutImage.imageView = m_renderScene->getSceneTextures().getBRDFLut()->getImageView();;
		lutImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

		m_renderer->vkDynamicDescriptorFactoryBegin(index)
			.bindImage(0,&lutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(m_descriptorSets[index],m_descriptorSetLayouts[index]);
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();
		plci.pushConstantRangeCount = 0; 

		std::vector<VkDescriptorSetLayout> setLayouts = {
			m_descriptorSetLayouts[index].layout
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/brdflut.comp.spv");
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

void engine::GpuBRDFLutPass::destroyPipeline()
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
