#ifdef FXAA_EFFECT

#include "fxaa.h"
#include "../renderer.h"

using namespace engine;

static AutoCVarInt32 cVarFXAA(
	"r.FXAA",
	"FXAA open or off, 0 is off, 1 is on. (Now deprecated)",
	"FXAA",
	0,
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

struct GpuPushConstants
{
	glm::vec2 invSize;
	glm::uvec2 size;
	float open;
};

void engine::FXAAPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::FXAAPass::beforeSceneTextureRecreate()
{
	destroyPipeline();
}

void engine::FXAAPass::afterSceneTextureRecreate()
{
	createPipeline();
}

void engine::FXAAPass::destroyPipeline()
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

void engine::FXAAPass::record(uint32 backBufferIndex)
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
	auto extent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();

	GpuPushConstants pushConst{};
	pushConst.invSize = { 1.0f / (float)extent.width, 1.0f / (float)extent.height };
	pushConst.size = { extent.width, extent.height };
	pushConst.open = cVarFXAA.get() == 0 ? 0.0f : 1.0f;
	vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], 
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GpuPushConstants), &pushConst);

	m_renderScene->getSceneTextures().getFXAA()->transitionLayout(cmd,VK_IMAGE_LAYOUT_GENERAL);
	vkCmdDispatch(cmd, getGroupCount(extent.width , 16),getGroupCount(extent.height, 16), 1);
	m_renderScene->getSceneTextures().getFXAA()->transitionLayout(cmd,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	commandBufEnd(backBufferIndex);
}

void engine::FXAAPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_descriptorSets.resize(backBufferCount);
	m_descriptorSetLayouts.resize(backBufferCount);

	for(uint32 index = 0; index < backBufferCount; index++)
	{
		VkDescriptorImageInfo outFXAAImage = {};
		outFXAAImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outFXAAImage.imageView = m_renderScene->getSceneTextures().getFXAA()->getImageView();;
		outFXAAImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

		VkDescriptorImageInfo inTonemapperImage = {};
		inTonemapperImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		inTonemapperImage.imageView = m_renderScene->getSceneTextures().getTonemapper()->getImageView();
		inTonemapperImage.sampler = VulkanRHI::get()->getLinearClampSampler(); // linear

		m_renderer->vkDynamicDescriptorFactoryBegin(index)
			.bindImage(0,&outFXAAImage,   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
			.bindImage(1,&inTonemapperImage,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(m_descriptorSets[index],m_descriptorSetLayouts[index]);
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(GpuPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		plci.pPushConstantRanges = &push_constant;
		plci.pushConstantRangeCount = 1; // NOTE: 我们有一个PushConstant

		std::vector<VkDescriptorSetLayout> setLayouts = {
			m_descriptorSetLayouts[index].layout
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/fxaa.comp.spv",true);
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

#endif