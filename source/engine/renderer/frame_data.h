#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"
#include "../shader_compiler/shader_compiler.h"

namespace engine{

constexpr int32 MAX_SSBO_OBJECTS      = 50000;
constexpr uint32 SSBO_BINDING_POS     = 0;

template<typename T> inline uint32 getSSBOIndex(){ CHECK(0); return 0; }

struct GPUObjectData
{
    __declspec(align(16)) glm::mat4 model;
};
template<> inline uint32 getSSBOIndex<GPUObjectData>(){ return 2; }

struct GPUMaterialData
{
    uint32 baseColorTexId;
    uint32 normalTexId;
    uint32 specularTexId;
    uint32 emissiveTexId;
};
template<> inline uint32 getSSBOIndex<GPUMaterialData>(){ return 3; }

struct GPUFrameData
{
	__declspec(align(16)) glm::vec4 appTime;
    __declspec(align(16)) glm::vec4 sunLightDir;
    __declspec(align(16)) glm::vec4 sunLightColor;

	__declspec(align(16)) glm::mat4 camView;
	__declspec(align(16)) glm::mat4 camProj;
	__declspec(align(16)) glm::mat4 camViewProj;
	__declspec(align(16)) glm::vec4 camWorldPos;
	__declspec(align(16)) glm::vec4 cameraInfo; // .x fovy .y aspect_ratio .z nearZ .w farZ
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

    void bindDescriptorSet(VkCommandBuffer cmd, VkPipelineLayout layout,VkPipelineBindPoint pt = VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,getSSBOIndex<SSBOType>(),1,&descriptorSets.set,0,nullptr);
    }

    void init()
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
            .bindBuffer(SSBO_BINDING_POS,&bufInfo,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(descriptorSets,descriptorSetLayout);
    }

    void release()
    {
        delete buffers;
    }
};

}