#include "irradiance_prefiltercube.h"
#include "../renderer.h"
#include "../texture.h"

using namespace engine;

struct GpuPushConstants
{
	uint32 envId;
	float exposure;
};

void engine::GpuIrradiancePrefilterPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::GpuIrradiancePrefilterPass::record(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

	std::vector<VkDescriptorSet> compPassSets = {
		m_descriptorSets[backBufferIndex].set,
		TextureLibrary::get()->getBindlessTextureDescriptorSet()
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

	auto envColorData = TextureLibrary::get()->getCombineTextureByName(s_defaultHdrEnvTextureName);
	GpuPushConstants pushConstants{};
	pushConstants.envId = envColorData.second.bindingIndex;
	pushConstants.exposure = getExposure();
	vkCmdPushConstants(cmd,m_pipelineLayouts[backBufferIndex],VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(GpuPushConstants),&pushConstants);
	
	vkCmdDispatch(cmd, SceneTextures::getPrefilterCubeDim() / 32, SceneTextures::getPrefilterCubeDim() / 32, 6);
	commandBufEnd(backBufferIndex);
}

void engine::GpuIrradiancePrefilterPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_descriptorSets.resize(backBufferCount);
	m_descriptorSetLayouts.resize(backBufferCount);
	for(uint32 index = 0; index < backBufferCount; index++)
	{
		VkDescriptorImageInfo filterImage = {};
		filterImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		filterImage.imageView = m_renderScene->getSceneTextures().getIrradiancePrefilterCube()->getImageView();;
		filterImage.sampler = VulkanRHI::get()->getPointClampEdgeSampler();

		m_renderer->vkDynamicDescriptorFactoryBegin(index)
			.bindImage(0,&filterImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(m_descriptorSets[index],m_descriptorSetLayouts[index]);
	}

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();
		plci.pushConstantRangeCount = 1;
		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(GpuPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		plci.pPushConstantRanges = &push_constant;

		std::vector<VkDescriptorSetLayout> setLayouts = {
			  m_descriptorSetLayouts[index].layout
			, TextureLibrary::get()->getBindlessTextureDescriptorSetLayout()
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/irradiancecube.comp.spv");
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

void engine::GpuIrradiancePrefilterPass::destroyPipeline()
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
