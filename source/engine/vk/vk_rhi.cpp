#include "vk_rhi.h"
#include "../core/timer.h"

namespace engine{

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	1,
	CVarFlags::InitOnce | CVarFlags::ReadOnly
);

constexpr auto MAX_TEXTURE_LOD = 24;

VulkanRHI* VulkanRHI::s_RHI = new VulkanRHI();

void VulkanRHI::init(GLFWwindow* window,
    std::vector<const char*> instanceLayerNames,
    std::vector<const char*> instanceExtensionNames,
    std::vector<const char*> deviceExtensionNames)
{
    if(ok) return;

    m_instanceExtensionNames = instanceExtensionNames;
    m_instanceExtensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // 用于VRAM监控
    m_instanceLayerNames = instanceLayerNames;
    m_deviceExtensionNames = deviceExtensionNames;
    m_deviceExtensionNames.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    m_deviceExtensionNames.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME); // 用于bindless
    m_deviceExtensionNames.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    m_deviceExtensionNames.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    // Enable gpu features here.
    m_enableGpuFeatures.samplerAnisotropy = true; // Enable sampler anisotropy.
    m_enableGpuFeatures.depthClamp = true;        // Depth clamp to avoid near plane clipping.
    m_enableGpuFeatures.shaderSampledImageArrayDynamicIndexing = true;
    m_enableGpuFeatures.multiDrawIndirect = VK_TRUE; // GPU Driven && Draw Indirect.

	m_physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    m_physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    m_physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
    m_physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
    m_physicalDeviceDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    m_physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    m_physicalDeviceDescriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

    m_window = window;
    m_instance.init(m_instanceExtensionNames,m_instanceLayerNames);
    m_deletionQueue.push([&]()
    {
        m_instance.destroy();
    });

    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS) 
    {
        LOG_GRAPHICS_FATAL("Window surface create error.");
    }
    m_deletionQueue.push([&]()
    {
        if(m_surface!=VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        }
    });

    m_deviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    m_device.init(m_instance,m_surface,m_enableGpuFeatures,m_deviceExtensionNames, &m_physicalDeviceDescriptorIndexingFeatures);
    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &m_physicalDeviceProperties);
    LOG_GRAPHICS_INFO("Gpu min align memory size：{0}.",m_physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);

	auto vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(vkGetInstanceProcAddr(m_instance,"vkGetPhysicalDeviceProperties2KHR"));
	assert(vkGetPhysicalDeviceProperties2KHR);
    VkPhysicalDeviceProperties2KHR deviceProperties{};
    m_descriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    deviceProperties.pNext = &m_descriptorIndexingProperties;
	vkGetPhysicalDeviceProperties2KHR(m_device.physicalDevice,&deviceProperties);

    m_swapchain.init(&m_device,&m_surface,window);
    createCommandPool();
    createSyncObjects();
    m_deletionQueue.push([&]()
    {
        releaseSyncObjects();
        releaseCommandPool();
        m_swapchain.destroy();
        m_device.destroy();
    });

    // Custom vulkan object cache and allocator.
    m_samplerCache.init(&m_device);
    m_shaderCache.init(m_device.device);
    m_descriptorLayoutCache.init(&m_device);
    m_staticDescriptorAllocator.init(&m_device);
    m_fencePool.init(m_device.device);

    createVmaAllocator();
    m_deletionQueue.push([&]()
    {
        releaseVmaAllocator();
        m_staticDescriptorAllocator.cleanup();
        m_descriptorLayoutCache.cleanup();
        m_shaderCache.release(); 
        m_samplerCache.cleanup();
        m_fencePool.release();
    });

    // 创建静态和动态的CommandBuffer
    createCommandBuffers();
    m_deletionQueue.push([&]()
    {
        releaseCommandBuffers();
    });

    ok = true;
}

static float vram_usage = 0.0f;
static float last_time = -1.0f;

float VulkanRHI::getVramUsage()
{
    if(last_time<0.0f)
    {
        last_time = g_timer.globalPassTime();
    }

    if(g_timer.globalPassTime()-last_time>1.0f)
    {
        last_time = g_timer.globalPassTime();

        VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget{};
        memBudget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
        memBudget.pNext = nullptr;

        VkPhysicalDeviceMemoryProperties2 memProps { };
        memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        memProps.pNext = &memBudget;

        vkGetPhysicalDeviceMemoryProperties2(m_device.physicalDevice, &memProps);

        int32 heapCount = memProps.memoryProperties.memoryHeapCount;
        int32 budgetSize=0;
        int32 usage=0;
        for(int32 i = 0; i < heapCount; i++)
        {
            budgetSize += (int32)memBudget.heapBudget[i];
            usage += (int32)memBudget.heapUsage[i];
        }

        vram_usage = float(usage) / float(budgetSize);
    }

    return vram_usage;
}

void VulkanRHI::release()
{
    if(!ok) return;
    m_deletionQueue.flush();
    ok = false;
}

uint32 VulkanRHI::acquireNextPresentImage()
{
    m_swapchainChange |= swapchainRebuild();

    vkWaitForFences(m_device,1,&m_inFlightFences[m_currentFrame],VK_TRUE,UINT64_MAX);
    VkResult result = vkAcquireNextImageKHR(
        m_device,m_swapchain,UINT64_MAX,
        m_semaphoresImageAvailable[m_currentFrame],
        VK_NULL_HANDLE,&m_imageIndex
    );

    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        LOG_GRAPHICS_FATAL("Fail to requeset present image.");
    }

    if(m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_device,1,&m_imagesInFlight[m_imageIndex],VK_TRUE,UINT64_MAX);
    }

    m_imagesInFlight[m_imageIndex] = m_inFlightFences[m_currentFrame];

    return m_imageIndex;
}

void VulkanRHI::present()
{
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    VkSemaphore signalSemaphores[] = { m_semaphoresRenderFinished[m_currentFrame] };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { m_swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &m_imageIndex;

    auto result = vkQueuePresentKHR(m_device.presentQueue,&present_info);
    if(result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR || m_swapchainChange)
    {
        m_swapchainChange = false;
        recreateSwapChain();
    }
    else if(result!=VK_SUCCESS)
    {
        LOG_GRAPHICS_FATAL("Fail to present image.");
    }

    m_currentFrame = (m_currentFrame +1) % m_maxFramesInFlight;
}

void VulkanRHI::submit(VkSubmitInfo& info)
{
    vkCheck(vkQueueSubmit(getGraphicsQueue(),1,&info,m_inFlightFences[m_currentFrame]));
}

void VulkanRHI::submit(VulkanSubmitInfo& info)
{
    vkCheck(vkQueueSubmit(getGraphicsQueue(),1,&info.get(),m_inFlightFences[m_currentFrame]));
}

void VulkanRHI::submit(uint32 count,VkSubmitInfo* infos)
{
    vkCheck(vkQueueSubmit(getGraphicsQueue(),count,infos,m_inFlightFences[m_currentFrame]));
}

void VulkanRHI::resetFence()
{
    vkCheck(vkResetFences(m_device,1,&m_inFlightFences[m_currentFrame]));
}

AsyncQueue* VulkanRHI::getAsyncCopyQueue()
{
    if(m_device.asyncTransferQueues.size() > 1)
    {
        return &m_device.asyncTransferQueues[1];
    }
    else if(m_device.asyncTransferQueues.size()==1)
    {
        return &m_device.asyncTransferQueues[0];
    }
    else if( m_device.asyncGraphicsQueues.size() > 0)
    {
        // 否则用倒数第一个闲置的图形队列
        return &m_device.asyncGraphicsQueues[m_device.asyncGraphicsQueues.size() - 1];
    }
    else
    {
        return nullptr;
    }
}

void VulkanRHI::submitAndResetFence(VkSubmitInfo& info)
{
    vkCheck(vkResetFences(m_device,1,&m_inFlightFences[m_currentFrame]));
    vkCheck(vkQueueSubmit(getGraphicsQueue(),1,&info,m_inFlightFences[m_currentFrame]));
}

void VulkanRHI::submitAndResetFence(VulkanSubmitInfo& info)
{
    vkCheck(vkResetFences(m_device,1,&m_inFlightFences[m_currentFrame]));
    vkCheck(vkQueueSubmit(getGraphicsQueue(),1,&info.get(),m_inFlightFences[m_currentFrame]));
}

void VulkanRHI::submitAndResetFence(uint32 count,VkSubmitInfo* infos)
{
    vkCheck(vkResetFences(m_device,1,&m_inFlightFences[m_currentFrame]));
    vkCheck(vkQueueSubmit(getGraphicsQueue(),count,infos,m_inFlightFences[m_currentFrame]));
}

VulkanShaderModule* VulkanRHI::getShader(const std::string& path)
{
    return m_shaderCache.getShader(path);
}

VkShaderModule VulkanRHI::getVkShader(const std::string& path)
{
    return m_shaderCache.getShader(path)->GetModule();
}

void VulkanRHI::addShaderModule(const std::string& path)
{
    m_shaderCache.getShader(path);
}

VkRenderPass VulkanRHI::createRenderpass(const VkRenderPassCreateInfo& info)
{
    VkRenderPass pass;
    vkCheck(vkCreateRenderPass(m_device, &info, nullptr, &pass));
    return pass;
}

VkPipelineLayout VulkanRHI::createPipelineLayout(const VkPipelineLayoutCreateInfo& info)
{
    VkPipelineLayout layout;
    vkCheck(vkCreatePipelineLayout(m_device,&info,nullptr,&layout));
    return layout;
}

void VulkanRHI::destroyRenderpass(VkRenderPass pass)
{
    vkDestroyRenderPass(m_device,pass,nullptr);
}

void VulkanRHI::destroyFramebuffer(VkFramebuffer fb)
{
    vkDestroyFramebuffer(m_device,fb,nullptr);
}

void VulkanRHI::destroyPipeline(VkPipeline pipe)
{
    vkDestroyPipeline(m_device,pipe,nullptr);
}

void VulkanRHI::destroyPipelineLayout(VkPipelineLayout layout)
{
    vkDestroyPipelineLayout(m_device,layout,nullptr);
}

size_t VulkanRHI::packUniformBufferOffsetAlignment(size_t originalSize) const
{
    size_t minUboAlignment = m_physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if(minUboAlignment > 0)
    {
        alignedSize = (alignedSize+minUboAlignment-1) &~ (minUboAlignment-1);
    }

    return alignedSize;
}

VkSampler VulkanRHI::createSampler(VkSamplerCreateInfo info)
{
    return m_samplerCache.createSampler(&info);
}



VkSampler VulkanRHI::getPointClampSampler()
{
    SamplerFactory sfa{};
    sfa
        .MagFilter(VK_FILTER_NEAREST)
        .MinFilter(VK_FILTER_NEAREST)
        .MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .AddressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .AddressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .AddressModeW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .CompareOp(VK_COMPARE_OP_LESS)
        .CompareEnable(VK_FALSE)
        .BorderColor(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
        .UnnormalizedCoordinates(VK_FALSE)
        .MaxAnisotropy(1.0f)
        .AnisotropyEnable(VK_FALSE)
        .MinLod(0.0f)
        .MaxLod(MAX_TEXTURE_LOD)
        .MipLodBias(0.0f);

    return VulkanRHI::get()->createSampler(sfa.getCreateInfo());
}

VkSampler VulkanRHI::getPointRepeatSampler()
{
    SamplerFactory sfa{};
    sfa
        .MagFilter(VK_FILTER_NEAREST)
        .MinFilter(VK_FILTER_NEAREST)
        .MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .AddressModeU(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .AddressModeV(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .AddressModeW(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .CompareOp(VK_COMPARE_OP_LESS)
        .CompareEnable(VK_FALSE)
        .BorderColor(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
        .UnnormalizedCoordinates(VK_FALSE)
        .MaxAnisotropy(1.0f)
        .AnisotropyEnable(VK_FALSE)
        .MinLod(0.0f)
        .MaxLod(MAX_TEXTURE_LOD)
        .MipLodBias(0.0f);

    return VulkanRHI::get()->createSampler(sfa.getCreateInfo());
}

VkSampler VulkanRHI::getLinearClampSampler()
{
    SamplerFactory sfa{};
    sfa.MagFilter(VK_FILTER_LINEAR)
        .MinFilter(VK_FILTER_LINEAR)
        .MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .AddressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .AddressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .AddressModeW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
        .CompareOp(VK_COMPARE_OP_LESS)
        .CompareEnable(VK_FALSE)
        .BorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
        .UnnormalizedCoordinates(VK_FALSE)
        .MaxAnisotropy(1.0f)
        .AnisotropyEnable(VK_FALSE)
        .MinLod(0.0f)
        .MaxLod(MAX_TEXTURE_LOD)
        .MipLodBias(0.0f);

    return VulkanRHI::get()->createSampler(sfa.getCreateInfo());
}

VkSampler VulkanRHI::getLinearRepeatSampler()
{
    SamplerFactory sfa{};
    sfa.MagFilter(VK_FILTER_LINEAR)
        .MinFilter(VK_FILTER_LINEAR)
        .MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .AddressModeU(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .AddressModeV(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .AddressModeW(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .CompareOp(VK_COMPARE_OP_LESS)
        .CompareEnable(VK_FALSE)
        .BorderColor(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
        .UnnormalizedCoordinates(VK_FALSE)
        .MaxAnisotropy(1.0f)
        .AnisotropyEnable(VK_FALSE)
        .MinLod(0.0f)
        .MaxLod(MAX_TEXTURE_LOD)
        .MipLodBias(0.0f);

    return VulkanRHI::get()->createSampler(sfa.getCreateInfo());
}

bool VulkanRHI::swapchainRebuild()
{
    static int current_width;
    static int current_height;
    glfwGetWindowSize(m_window,&current_width,&current_height);

    static int last_width = current_width;
    static int last_height = current_height;

    if(current_width != last_width || current_height != last_height)
    {
        last_width = current_width;
        last_height = current_height;
        return true;
    }

    return false;
}

void VulkanRHI::createCommandPool()
{
    auto queueFamilyIndices = m_device.findQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS) 
    {
        LOG_GRAPHICS_FATAL("Fail to create vulkan CommandPool.");
    }

    poolInfo.queueFamilyIndex =  queueFamilyIndices.computeFaimly;
    if(vkCreateCommandPool(m_device,&poolInfo,nullptr,&m_computeCommandPool)!=VK_SUCCESS)
    {
        LOG_GRAPHICS_FATAL("Fail to create compute CommandPool.");
    }

    VkCommandPoolCreateInfo copyPoolInfo{};
    copyPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    copyPoolInfo.queueFamilyIndex = queueFamilyIndices.transferFamily;
    copyPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if(vkCreateCommandPool(m_device,&poolInfo,nullptr,&m_copyCommandPool)!=VK_SUCCESS)
    {
        LOG_GRAPHICS_FATAL("Fail to create copy CommandPool.");
    }
}

void VulkanRHI::createSyncObjects()
{
    const auto imageNums = m_swapchain.getImageViews().size();
    m_semaphoresImageAvailable.resize(m_maxFramesInFlight);
    m_semaphoresRenderFinished.resize(m_maxFramesInFlight);
    m_staticGraphicsCommandExecuteSemaphores.resize(imageNums);
    m_dynamicGraphicsCommandExecuteSemaphores.resize(imageNums);
    m_inFlightFences.resize(m_maxFramesInFlight);
    m_imagesInFlight.resize(imageNums);
    for(auto& fence : m_imagesInFlight)
    {
        fence = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < m_maxFramesInFlight; i++)
    {
        if(
            vkCreateSemaphore(m_device,&semaphoreInfo,nullptr,&m_semaphoresImageAvailable[i])!=VK_SUCCESS||
            vkCreateSemaphore(m_device,&semaphoreInfo,nullptr,&m_semaphoresRenderFinished[i])!=VK_SUCCESS||
            vkCreateSemaphore(m_device,&semaphoreInfo,nullptr,&m_staticGraphicsCommandExecuteSemaphores[i])!=VK_SUCCESS||
            vkCreateSemaphore(m_device,&semaphoreInfo,nullptr,&m_dynamicGraphicsCommandExecuteSemaphores[i])!=VK_SUCCESS||
            vkCreateFence(m_device,&fenceInfo,nullptr,&m_inFlightFences[i])!=VK_SUCCESS)
        {
            LOG_GRAPHICS_FATAL("Fail to create semaphore.");
        }
    }
}

void VulkanRHI::createVmaAllocator()
{
    if(CVarSystem::get()->getInt32CVar("r.RHI.EnableVma"))
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_device.physicalDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;
        vmaCreateAllocator(&allocatorInfo, &m_vmaAllocator);
    }
}

void VulkanRHI::createCommandBuffers()
{
    CHECK(m_staticGraphicCommandBuffer.size() == 0);
    CHECK(m_dynamicGraphicsCommandBuffer.size() == 0);

    auto size = m_swapchain.getImageViews().size();
    m_staticGraphicCommandBuffer.resize(size);
    m_dynamicGraphicsCommandBuffer.resize(size);

    for(size_t i = 0; i < size; i++)
    {
        m_staticGraphicCommandBuffer[i] = createGraphicsCommandBuffer();
        m_dynamicGraphicsCommandBuffer[i] = createGraphicsCommandBuffer();
    }
}

void VulkanRHI::releaseCommandBuffers()
{
    CHECK(m_staticGraphicCommandBuffer.size() >= 0);

    for(size_t i = 0; i < m_staticGraphicCommandBuffer.size(); i++)
    {
        delete m_staticGraphicCommandBuffer[i];
        delete m_dynamicGraphicsCommandBuffer[i];
    }
    m_staticGraphicCommandBuffer.resize(0);
    m_dynamicGraphicsCommandBuffer.resize(0);
}

void VulkanRHI::releaseVmaAllocator()
{
    if(CVarSystem::get()->getInt32CVar("r.RHI.EnableVma"))
    {
        vmaDestroyAllocator(m_vmaAllocator);
    }
}

VulkanCommandBuffer* VulkanRHI::createGraphicsCommandBuffer(VkCommandBufferLevel level)
{
    return VulkanCommandBuffer::create(
        &m_device,
        m_graphicsCommandPool,
        level,
        m_device.graphicsQueue
    );
}

VulkanCommandBuffer* VulkanRHI::createComputeCommandBuffer(VkCommandBufferLevel level)
{
    return VulkanCommandBuffer::create(
        &m_device,
        m_computeCommandPool,
        level,
        m_device.computeQueue
    );
}

VulkanCommandBuffer* VulkanRHI::createCopyCommandBuffer(VkCommandBufferLevel level)
{
    return VulkanCommandBuffer::create(
        &m_device,
        m_copyCommandPool,
        level,
        getAsyncCopyQueue()->queue
    );
}

void VulkanRHI::releaseCommandPool()
{
    if(m_graphicsCommandPool!=VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);
    }
    if(m_computeCommandPool!=VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_computeCommandPool, nullptr);
    }
    if(m_copyCommandPool!=VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_copyCommandPool, nullptr);
    }
}

void VulkanRHI::releaseSyncObjects()
{
    for(size_t i = 0; i < m_maxFramesInFlight; i++)
    {
        vkDestroySemaphore(m_device,m_semaphoresImageAvailable[i],nullptr);
        vkDestroySemaphore(m_device,m_semaphoresRenderFinished[i],nullptr);
        vkDestroySemaphore(m_device,m_staticGraphicsCommandExecuteSemaphores[i],nullptr);
        vkDestroySemaphore(m_device,m_dynamicGraphicsCommandExecuteSemaphores[i],nullptr);
        vkDestroyFence(m_device,m_inFlightFences[i],nullptr);
    }
}

void VulkanRHI::recreateSwapChain()
{
    static int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) 
    {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);

    // Clean special
    for(auto& callBackPair : m_callbackBeforeSwapchainRebuild)
    {
        callBackPair.second();
    }

    releaseCommandBuffers();
    releaseSyncObjects();
    m_swapchain.destroy();
    m_swapchain.init(&m_device,&m_surface,m_window);
    createSyncObjects();
    createCommandBuffers();
    m_imagesInFlight.resize(m_swapchain.getImageViews().size(), VK_NULL_HANDLE);

    // Recreate special
    for(auto& callBackPair : m_callbackAfterSwapchainRebuild)
    {
        callBackPair.second();
    }
}
}