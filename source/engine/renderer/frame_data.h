#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"
#include "../shader_compiler/shader_compiler.h"

namespace engine{

struct GPUFrameData
{
	__declspec(align(16)) glm::vec4 appTime;
    __declspec(align(16)) glm::vec4 sunLightDir;
    __declspec(align(16)) glm::vec4 sunLightColor;
};

struct GPUViewData
{
	__declspec(align(16)) glm::mat4 view;
	__declspec(align(16)) glm::mat4 proj;
	__declspec(align(16)) glm::mat4 viewProj;
	__declspec(align(16)) glm::vec4 worldPos;
};

class Renderer;
class PerFrameData
{
public:
    VulkanBuffer* m_frameDataBuffer = nullptr;
    VulkanBuffer* m_viewDataBuffer = nullptr;

    std::vector<VulkanDescriptorSetReference> m_frameDataDescriptorSets = {};
    std::vector<VulkanDescriptorSetReference> m_viewDataDescriptorSets = {};
    std::vector<VulkanDescriptorLayoutReference> m_frameDataDescriptorSetLayouts = {};
    std::vector<VulkanDescriptorLayoutReference> m_viewDataDescriptorSetLayouts = {};

    void createGPUBuffer();
    void releaseGPUBuffer();

    bool bNeedRebuild = true;
    

public:
    void buildPerFrameDataDescriptorSets(Ref<Renderer>);

    void init()
    {
        createGPUBuffer();
    }

    void release()
    {
        releaseGPUBuffer();
    }

    void markPerframeDescriptorSetsDirty() { bNeedRebuild = true; }
    
    VkBuffer& getFrameDataBuffer() { return m_frameDataBuffer->GetVkBuffer(); }
    VkBuffer& getViewDataBuffer() { return m_viewDataBuffer->GetVkBuffer(); }

    void updateFrameData(const GPUFrameData& in);
    void updateViewData(const GPUViewData& in);
};

struct SceneUploadSSBO
{
    size_t getSSBOSize() const;

    VulkanBuffer* objectBuffers = nullptr;
    VulkanDescriptorSetReference objectDescriptorSets = {};
    VulkanDescriptorLayoutReference objectDescriptorSetLayout = {};

    void bindDescriptorSet(VkCommandBuffer cmd, VkPipelineLayout layout, VkPipelineBindPoint pt = VK_PIPELINE_BIND_POINT_GRAPHICS);

    void init();
    void release();
};

}