#pragma once
#include "impl/vk_device.h"
#include "impl/vk_buffer.h"
#include "impl/vk_cmdbuffer.h"
#include "impl/vk_common.h"
#include "impl/vk_descriptor.h"
#include "impl/vk_factory.h"
#include "impl/vk_image.h"
#include "impl/vk_instance.h"
#include "impl/vk_sampler.h"
#include "impl/vk_shader.h"
#include "impl/vk_swapchain.h"
#include "../core/deletion_queue.h"

namespace engine{

class VulkanRHI
{
private:
    class VulkanShaderCache
    {
    public:
        VulkanShaderModule* getShader(const std::string& path)
        {
            auto it = m_moduleCache.find(path);
            if(it == m_moduleCache.end())
            {
                VulkanShaderModule* newShader = VulkanShaderModule::create(m_device,path);
                m_moduleCache[path] = newShader;
            }
            return m_moduleCache[path];
        }

        void release()
        {
            for (auto shaders : m_moduleCache)
            {
                delete shaders.second;
            }
        }

        void init(VkDevice device)
        {
            m_device = device;
        };

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        std::unordered_map<std::string,VulkanShaderModule*> m_moduleCache;
    };

public:
    typedef void (RegisterFuncBeforeSwapchainRecreate)();
    typedef void (RegisterFuncAfterSwapchainRecreate)();
private:
    bool ok = false;
    static VulkanRHI* s_RHI;
    bool m_swapchainChange = false;
    GLFWwindow* m_window;

private:
    std::vector<const char*> m_instanceLayerNames = {};
    std::vector<const char*> m_instanceExtensionNames = {};
    std::vector<const char*> m_deviceExtensionNames = {};
    VkPhysicalDeviceFeatures m_enableGpuFeatures = {};
    VkPhysicalDeviceProperties m_physicalDeviceProperties = {};

private:
    VulkanSwapchain m_swapchain = {};
    VulkanDevice m_device = {};
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VulkanInstance m_instance = {};
    VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool m_computeCommandPool = VK_NULL_HANDLE;
    DeletionQueue m_deletionQueue = {};

    VmaAllocator m_vmaAllocator = {};

private: // cache
    VulkanShaderCache m_shaderCache = {};
    VulkanDescriptorAllocator m_descriptorAllocator = {};
    VulkanDescriptorLayoutCache m_descriptorLayoutCache = {};

private:
    uint32 m_imageIndex;
    uint32 m_currentFrame = 0;
    const int m_maxFramesInFlight = 3;
    std::vector<VkSemaphore> m_semaphoresImageAvailable;
    std::vector<VkSemaphore> m_semaphoresRenderFinished;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;

    std::unordered_map<std::string,std::function<RegisterFuncBeforeSwapchainRecreate>> m_callbackBeforeSwapchainRebuild = {};
    std::unordered_map<std::string,std::function<RegisterFuncBeforeSwapchainRecreate>> m_callbackAfterSwapchainRebuild = {};
    VulkanRHI() { }


public:
    ~VulkanRHI(){ release(); }
    static VulkanRHI* get() { return s_RHI; }
    void init(
        GLFWwindow* window,
        std::vector<const char*> instanceLayerNames = {},
        std::vector<const char*> instanceExtensionNames = {},
        std::vector<const char*> deviceExtensionNames = {}
    );

    VkFormat findDepthStencilFormat() { return m_device.findDepthStencilFormat(); }
    void release();
    uint32 acquireNextPresentImage();
    void present();
    void waitIdle(){ vkDeviceWaitIdle(m_device); }
    void submitAndResetFence(VkSubmitInfo& info);
    void submitAndResetFence(VulkanSubmitInfo& info);
    void submitAndResetFence(uint32 count,VkSubmitInfo* infos);
    void submit(VkSubmitInfo& info);
    void submit(VulkanSubmitInfo& info);
    void submit(uint32 count,VkSubmitInfo* infos);
    void resetFence();
    VkPhysicalDeviceFeatures& getVkPhysicalDeviceFeatures() { return m_enableGpuFeatures; }
    VmaAllocator& getVmaAllocator() { return m_vmaAllocator; }
    VkQueue& getGraphicsQueue() { return m_device.graphicsQueue; }
    VkQueue& getComputeQueue() { return m_device.computeQueue; }
    const VkDevice& getDevice() const { return m_device.device; }
    VulkanDevice* getVulkanDevice() { return &m_device; }
    VkCommandPool& getGraphicsCommandPool() { return m_graphicsCommandPool; }
    VkCommandPool& getComputeCommandPool() { return m_computeCommandPool; }
    VkInstance getInstance() { return m_instance; }
    VulkanVertexBuffer* createVertexBuffer(const std::vector<float>& data,std::vector<EVertexAttribute>&& as) = delete;
    VulkanVertexBuffer* createVertexBuffer(const std::vector<float>& data,const std::vector<EVertexAttribute>& as) 
    { 
        return VulkanVertexBuffer::create(&m_device,m_graphicsCommandPool,data,as); 
    }

    VulkanIndexBuffer* createIndexBuffer(const std::vector<VertexIndexType>& data)
    {
        return VulkanIndexBuffer::create(&m_device,m_graphicsCommandPool,data);
    }

    VulkanShaderCache& getShaderCache() { return m_shaderCache; }
    VulkanShaderModule* getShader(const std::string& path);
    VkShaderModule getVkShader(const std::string& path);
    void addShaderModule(const std::string& path);
    const uint32 getCurrentFrameIndex() { return m_currentFrame; }
    VkSemaphore* getCurrentFrameWaitSemaphore() { return &m_semaphoresImageAvailable[m_currentFrame]; }
    VkSemaphore* getCurrentFrameFinishSemaphore() { return &m_semaphoresRenderFinished[m_currentFrame]; }
    VulkanSwapchain& getSwapchain() { return m_swapchain; }
    std::vector<VkImageView>& getSwapchainImageViews() { return m_swapchain.getImageViews(); }
    std::vector<VkImage>& getSwapchainImages() { return m_swapchain.getImages(); }
    VkFormat getSwapchainFormat() const { return m_swapchain.getSwapchainImageFormat();  }
    VulkanCommandBuffer* createGraphicsCommandBuffer(VkCommandBufferLevel level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VulkanCommandBuffer* createComputeCommandBuffer(VkCommandBufferLevel level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkRenderPass createRenderpass(const VkRenderPassCreateInfo& info);
    VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& info);
    VulkanDescriptorFactory vkDescriptorFactoryBegin() { return VulkanDescriptorFactory::begin(&m_descriptorLayoutCache,&m_descriptorAllocator); }
    void destroyRenderpass(VkRenderPass pass);
    void destroyFramebuffer(VkFramebuffer fb);
    void destroyPipeline(VkPipeline pipe);
    void destroyPipelineLayout(VkPipelineLayout layout);
    VkPhysicalDeviceProperties getPhysicalDeviceProperties() const { return m_physicalDeviceProperties; }
    size_t packUniformBufferOffsetAlignment(size_t originalSize) const;
    template <typename T> size_t getUniformBufferPadSize() const{ return PackUniformBufferOffsetAlignment(sizeof(T)); }
    VkExtent2D getSwapchainExtent() const { return m_swapchain.getSwapchainExtent();  }

public:
    void addBeforeSwapchainRebuildCallback(std::string name,const std::function<RegisterFuncAfterSwapchainRecreate>& func)
    {
        this->m_callbackBeforeSwapchainRebuild[name] = func;
    }
    void removeBeforeSwapchainRebuildCallback(std::string name)
    {
        this->m_callbackBeforeSwapchainRebuild.erase(name);
    }

    void addAfterSwapchainRebuildCallback(std::string name,const std::function<RegisterFuncAfterSwapchainRecreate>& func)
    {
        this->m_callbackAfterSwapchainRebuild[name] = func;
    }
    void removeAfterSwapchainRebuildCallback(std::string name)
    {
        this->m_callbackAfterSwapchainRebuild.erase(name);
    }

private:
    bool swapchainRebuild();
    void createCommandPool();
    void createSyncObjects();
    void createVmaAllocator();

    void releaseCommandPool();
    void releaseSyncObjects();
    void recreateSwapChain();
    void releaseVmaAllocator();
};

}