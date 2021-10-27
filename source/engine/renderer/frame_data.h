#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"
#include "../shader_compiler/shader_compiler.h"

namespace engine{

constexpr int32 MAX_SSBO_OBJECTS      = 50000;

struct GPUObjectData
{
    glm::mat4 model;
    glm::vec4 sphereBounds;
    glm::vec4 extents;

    uint32 indexCount; // pad 4 uint32
    uint32 firstIndex;
    uint32 vertexOffset;
    uint32 firstInstance;
};

struct GPUMaterialData
{
    uint32 baseColorTexId;
    uint32 normalTexId;
    uint32 specularTexId;
    uint32 emissiveTexId;
};

struct GPUFrameData
{
	glm::vec4 appTime;
    glm::vec4 sunLightDir;
    glm::vec4 sunLightColor;

	glm::mat4 camView;
	glm::mat4 camProj;
	glm::mat4 camViewProj;
    glm::vec4 camWorldPos;
	glm::vec4 cameraInfo; // .x fovy .y aspect_ratio .z nearZ .w farZ
    glm::vec4 camFrustumPlanes[6];
};

class Renderer;
class PerFrameData
{
public:
    VulkanBuffer* m_frameDataBuffer = nullptr;

    std::vector<VulkanDescriptorSetReference> m_frameDataDescriptorSets = {};
    std::vector<VulkanDescriptorLayoutReference> m_frameDataDescriptorSetLayouts = {};

    void createGPUBuffer();
    void releaseGPUBuffer();

    bool bNeedRebuild = true;

public:
    void buildPerFrameDataDescriptorSets(Ref<Renderer>);

    inline void init()
    {
        createGPUBuffer();
    }

    void release()
    {
        releaseGPUBuffer();
    }

    void markPerframeDescriptorSetsDirty() { bNeedRebuild = true; }
    
    VkBuffer& getFrameDataBuffer() { return m_frameDataBuffer->GetVkBuffer(); }
    void updateFrameData(const GPUFrameData& in);
};

template<typename SSBOType>
struct SceneUploadSSBO
{
    VulkanBuffer* buffers = nullptr;
    VulkanDescriptorSetReference descriptorSets = {};
    VulkanDescriptorLayoutReference descriptorSetLayout = {};

    size_t getSSBOSize() const
    {
        const size_t bufferSize = sizeof(SSBOType) * MAX_SSBO_OBJECTS;
        return bufferSize;
    }

    void init(uint32 bindingPos)
    {
        auto bufferSize = getSSBOSize();

        buffers = VulkanBuffer::create(
            VulkanRHI::get()->getVulkanDevice(),
            VulkanRHI::get()->getGraphicsCommandPool(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize,
            nullptr
        );

        VkDescriptorBufferInfo bufInfo = {};
        bufInfo.buffer = *buffers;
        bufInfo.offset = 0;
        bufInfo.range = bufferSize;

        VulkanRHI::get()->vkDescriptorFactoryBegin()
            .bindBuffer(bindingPos,&bufInfo,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT )
            .build(descriptorSets,descriptorSetLayout);
    }

    void release()
    {
        delete buffers;
    }
};

}