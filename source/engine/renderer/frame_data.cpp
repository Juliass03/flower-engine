#include "frame_data.h"
#include "renderer.h"
#include "render_scene.h"

using namespace engine;

void engine::PerFrameData::createGPUBuffer()
{
    constexpr size_t frameDataBufferSize = sizeof(GPUFrameData);
    m_frameDataBuffer = VulkanBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        frameDataBufferSize,
        nullptr
    );
}

void engine::PerFrameData::releaseGPUBuffer()
{
    delete m_frameDataBuffer;
}

void engine::PerFrameData::buildPerFrameDataDescriptorSets(Ref<Renderer> renderer)
{
    if(bNeedRebuild)
    {
        uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
        m_frameDataDescriptorSets.resize(backBufferCount);
        m_frameDataDescriptorSetLayouts.resize(backBufferCount);

        for(uint32 index = 0; index < backBufferCount; index++)
        {
            VkDescriptorBufferInfo frameDataBufInfo = {};
            frameDataBufInfo.buffer = getFrameDataBuffer();
            frameDataBufInfo.offset = 0;
            frameDataBufInfo.range = sizeof(GPUFrameData);

            renderer->vkDynamicDescriptorFactoryBegin(index)
                .bindBuffer(0,&frameDataBufInfo,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(m_frameDataDescriptorSets[index],m_frameDataDescriptorSetLayouts[index]);
        }
        bNeedRebuild = false;
    }
}

void engine::PerFrameData::updateFrameData(const GPUFrameData& in)
{
    GPUFrameData frameData = in;
    auto size = sizeof(GPUFrameData);
    m_frameDataBuffer->map(size);
    m_frameDataBuffer->copyTo(&frameData,size);
    m_frameDataBuffer->unmap();
}
