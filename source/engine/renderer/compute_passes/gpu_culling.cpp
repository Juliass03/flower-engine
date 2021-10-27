#include "gpu_culling.h"
#include "../renderer.h"

using namespace engine;

void engine::GpuCullingPass::initInner()
{
	bInitPipeline = false;
    createPipeline();
    createAsyncObjects();

    m_deletionQueue.push([&]()
    {
        destroyPipeline();
        destroyAsyncObjects();
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

void engine::GpuCullingPass::record(uint32 index)
{
    m_cmdbufs[index]->begin();

    VkCommandBuffer cmd = *m_cmdbufs[index];

    GPUCullingPushConstants gpuPushConstant = {};
    gpuPushConstant.drawCount = (uint32)m_renderScene->m_cacheMeshObjectSSBOData.size();

    VkDeviceSize asyncRange =  gpuPushConstant.drawCount * sizeof(VkDrawIndexedIndirectCommand);
    asyncRange = asyncRange > 0 ? asyncRange : 256;

    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.buffer = m_renderScene->m_drawIndirectSSBOGbuffer.drawIndirectSSBO->GetVkBuffer();
    bufferBarrier.size = asyncRange;
    bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.srcQueueFamilyIndex = VulkanRHI::get()->getVulkanDevice()->graphicsFamily;
    bufferBarrier.dstQueueFamilyIndex = VulkanRHI::get()->getVulkanDevice()->computeFamily;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        1, &bufferBarrier,
        0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[index]);

    vkCmdPushConstants(cmd, m_pipelineLayouts[index], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &gpuPushConstant);
    
    std::vector<VkDescriptorSet> compPassSets = {
        m_renderer->getFrameData().m_frameDataDescriptorSets[index].set
        , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSets.set
    };

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        m_pipelineLayouts[index], 
        0, 
        (uint32)compPassSets.size(), 
        compPassSets.data(), 
        0, 
        nullptr
    );

    vkCmdDispatch(cmd, (gpuPushConstant.drawCount / 256) + 1, 1, 1);

    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferBarrier.buffer = m_renderScene->m_drawIndirectSSBOGbuffer.drawIndirectSSBO->GetVkBuffer();
    bufferBarrier.size = asyncRange;
    bufferBarrier.srcQueueFamilyIndex = VulkanRHI::get()->getVulkanDevice()->computeFamily;
    bufferBarrier.dstQueueFamilyIndex = VulkanRHI::get()->getVulkanDevice()->graphicsFamily;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0,
        0, nullptr,
        1, &bufferBarrier,
        0, nullptr);

    m_cmdbufs[index]->end();
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
        };

        plci.setLayoutCount = (uint32)setLayouts.size();
        plci.pSetLayouts = setLayouts.data();

        m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

        auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/gpuculling.comp.spv");
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

void engine::GpuCullingPass::createAsyncObjects()
{
    uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
    m_semaphores.resize(backBufferCount);
    m_cmdbufs.resize(backBufferCount);

    for(uint32 index = 0; index < backBufferCount; index++)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCheck(vkCreateSemaphore(VulkanRHI::get()->getDevice(), &semaphoreCreateInfo, nullptr, &m_semaphores[index]));

        m_cmdbufs[index] = VulkanRHI::get()->createComputeCommandBuffer();
    }
}

void engine::GpuCullingPass::destroyAsyncObjects()
{
    for(uint32 index = 0; index < m_cmdbufs.size(); index++)
    {
        vkDestroySemaphore(VulkanRHI::get()->getDevice(), m_semaphores[index], nullptr);
        
        delete m_cmdbufs[index];
        m_cmdbufs[index] = nullptr;
    }
    m_semaphores.resize(0);
    m_cmdbufs.resize(0);

}
