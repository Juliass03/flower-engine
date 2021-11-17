#include "gpu_culling.h"
#include "../renderer.h"

using namespace engine;

void engine::GpuCullingPass::initInner()
{
	bInitPipeline = false;
    createPipeline();

    m_deletionQueue.push([&]()
    {
		destroyPipeline();
    });
}

void engine::GpuCullingPass::beforeSceneTextureRecreate()
{
    destroyPipeline();
}

void engine::GpuCullingPass::afterSceneTextureRecreate()
{
    createPipeline();
}

void engine::GpuCullingPass::gbuffer_record(uint32 backBufferIndex)
{
    VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
    commandBufBegin(backBufferIndex);

	GPUCullingPushConstants gpuPushConstant = {};
	gpuPushConstant.drawCount = (uint32)m_renderScene->m_cacheMeshObjectSSBOData.size();
    gpuPushConstant.cullIndex = static_cast<uint32>(ECullIndex::GBUFFER);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

    vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCullingPushConstants), &gpuPushConstant);
    
    std::vector<VkDescriptorSet> compPassSets = {
          m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set
        , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.countDescriptorSets.set
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

    vkCmdDispatch(cmd, (gpuPushConstant.drawCount / 256) + 1, 1, 1);
    
    commandBufEnd(backBufferIndex);
}

void engine::GpuCullingPass::cascade_record(uint32 backBufferIndex)
{
    VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
    commandBufBegin(backBufferIndex);

    for(size_t i = 0; i < CASCADE_MAX_COUNT; i++)
    {
        cascade_record(cmd,backBufferIndex,ECullIndex(i + 1));
    }

    commandBufEnd(backBufferIndex);
}

void engine::GpuCullingPass::cascade_record(VkCommandBuffer cmd,uint32 backBufferIndex,ECullIndex cullIndex)
{
    uint32 cascasdeIndex = cullingIndexToCasacdeIndex(cullIndex);

    GPUCullingPushConstants gpuPushConstant = {};
    gpuPushConstant.drawCount = (uint32)m_renderScene->m_cacheMeshObjectSSBOData.size();
    gpuPushConstant.cullIndex = static_cast<uint32>(cullIndex); 
   
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

    vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCullingPushConstants), &gpuPushConstant);

    std::vector<VkDescriptorSet> compPassSets = {
          m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set
        , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOShadowDepths[cascasdeIndex].descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOShadowDepths[cascasdeIndex].countDescriptorSets.set
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

    vkCmdDispatch(cmd, (gpuPushConstant.drawCount / 256) + 1, 1, 1);
}

void engine::GpuCullingPass::createPipeline()
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
        push_constant.size = sizeof(GPUCullingPushConstants);
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        plci.pPushConstantRanges = &push_constant;
        plci.pushConstantRangeCount = 1; // NOTE: 我们有一个PushConstant

        std::vector<VkDescriptorSetLayout> setLayouts = {
              m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout
            , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSetLayout.layout
            , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSetLayout.layout
            , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.countDescriptorSetLayout.layout
        };

        plci.setLayoutCount = (uint32)setLayouts.size();
        plci.pSetLayouts = setLayouts.data();

        m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

        auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/gpuculling.comp.spv",true);
        VkPipelineShaderStageCreateInfo shaderStageCI{};
        shaderStageCI.module =  shaderModule->GetModule();
        shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageCI.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = m_pipelineLayouts[index];
        computePipelineCreateInfo.flags = 0;
        computePipelineCreateInfo.stage = shaderStageCI;
        vkCheck(vkCreateComputePipelines(VulkanRHI::get()->getDevice(), nullptr, 1, &computePipelineCreateInfo, nullptr, &m_pipelines[index]));
    }

    bInitPipeline = true;
}

void engine::GpuCullingPass::destroyPipeline()
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
