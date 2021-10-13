#include "frame_data.h"
#include "renderer.h"
#include "render_scene.h"

using namespace engine;

const int MAX_SSBO_OBJECTS = 50000;
const int SSBO_SET_POSITION = 2;
const int SSBO_BINDING_POSITION = 0;

void engine::PerFrameData::createGPUBuffer()
{
    const size_t frameDataBufferSize = sizeof(GPUFrameData);
    const size_t ViewDataBufferSize  = sizeof(GPUViewData);

    m_frameDataBuffer = VulkanBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        frameDataBufferSize,
        nullptr
    );

    m_viewDataBuffer = VulkanBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ViewDataBufferSize,
        nullptr
    );
}

void engine::PerFrameData::releaseGPUBuffer()
{
    delete m_frameDataBuffer;
    delete m_viewDataBuffer;
}

void engine::PerFrameData::buildPerFrameDataDescriptorSets(Ref<Renderer> renderer)
{
    if(bNeedRebuild)
    {
        uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
        m_frameDataDescriptorSets.resize(backBufferCount);
        m_viewDataDescriptorSets.resize(backBufferCount);
        m_frameDataDescriptorSetLayouts.resize(backBufferCount);
        m_viewDataDescriptorSetLayouts.resize(backBufferCount);

        for(uint32 index = 0; index < backBufferCount; index++)
        {
            VkDescriptorBufferInfo frameDataBufInfo = {};
            frameDataBufInfo.buffer = getFrameDataBuffer();
            frameDataBufInfo.offset = 0;
            frameDataBufInfo.range = sizeof(GPUFrameData);

            renderer->vkDynamicDescriptorFactoryBegin(index)
                .bindBuffer(0,&frameDataBufInfo,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(m_frameDataDescriptorSets[index],m_frameDataDescriptorSetLayouts[index]);

            VkDescriptorBufferInfo viewDataBufInfo = {};
            viewDataBufInfo.buffer = getViewDataBuffer();
            viewDataBufInfo.offset = 0;
            viewDataBufInfo.range = sizeof(GPUViewData);

            renderer->vkDynamicDescriptorFactoryBegin(index)
                .bindBuffer(0,&viewDataBufInfo,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(m_viewDataDescriptorSets[index],m_viewDataDescriptorSetLayouts[index]);
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

void engine::PerFrameData::updateViewData(const GPUViewData& in)
{
    GPUViewData viewData = in;
    auto size = sizeof(GPUViewData);
    m_viewDataBuffer->map(size);
    m_viewDataBuffer->copyTo(&viewData,size);
    m_viewDataBuffer->unmap();
}

size_t SceneUploadSSBO::getSSBOSize() const
{
    const size_t objectBufferSize = sizeof(GPUObjectData) * MAX_SSBO_OBJECTS;
    return objectBufferSize;
}

void engine::SceneUploadSSBO::bindDescriptorSet(VkCommandBuffer cmd,VkPipelineLayout layout,VkPipelineBindPoint pt)
{
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout,
        SSBO_SET_POSITION,
        1,
        &objectDescriptorSets.set,
        0,
        nullptr
    );
}

void engine::SceneUploadSSBO::init()
{
    auto objectBufferSize = getSSBOSize();

    objectBuffers = VulkanBuffer::create(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        objectBufferSize,
        nullptr
    );

    VkDescriptorBufferInfo objBufInfo = {};
    objBufInfo.buffer = *objectBuffers;
    objBufInfo.offset = 0;
    objBufInfo.range = objectBufferSize;

    VulkanRHI::get()->vkDescriptorFactoryBegin()
        .bindBuffer(SSBO_BINDING_POSITION,&objBufInfo,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(objectDescriptorSets,objectDescriptorSetLayout);
}

void engine::SceneUploadSSBO::release()
{
    delete objectBuffers;
}
