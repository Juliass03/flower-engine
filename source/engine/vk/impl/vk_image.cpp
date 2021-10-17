#include "vk_image.h"
#include "vk_buffer.h"
#include "../vk_rhi.h"

namespace engine{

void VulkanImage::release()
{
    if( m_allocation != nullptr )
    {
        if (m_image != VK_NULL_HANDLE) 
        {
            vmaDestroyImage(VulkanRHI::get()->getVmaAllocator(),m_image,m_allocation);
            m_image = VK_NULL_HANDLE;
        }
    }
    else
    {
        if (m_image != VK_NULL_HANDLE) 
        {
            vkDestroyImage(*m_device, m_image, nullptr); 
            m_image = VK_NULL_HANDLE;
        }
    }

    if (m_memory != VK_NULL_HANDLE) 
    {
        vkFreeMemory(*m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

    if (m_imageView != VK_NULL_HANDLE) 
    {
        vkDestroyImageView(*m_device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
}

void VulkanImage::transitionLayout(VkCommandBuffer cb,VkImageLayout newLayout,VkImageAspectFlags aspectFlags)
{
    if(newLayout == m_currentLayout)
        return;

    VkImageLayout oldLayout = m_currentLayout;
    m_currentLayout = newLayout;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;

    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_createInfo.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_createInfo.arrayLayers;

    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkAccessFlags srcMask{};
    VkAccessFlags dstMask{};
    VkDependencyFlags dependencyFlags{};

    switch(oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        srcMask = 0;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        srcMask = VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        srcMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        srcMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        srcMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        srcMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        srcMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        LOG_GRAPHICS_FATAL("Image layout transition no support.");
        srcMask = 0;
        break;
    }

    switch(newLayout)
    {
    case VK_IMAGE_LAYOUT_GENERAL:
        dstMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        dstMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        dstMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        dstMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        dstMask = dstMask|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        if(srcMask==0)
        {
            // Undefined
            srcMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        dstMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        LOG_GRAPHICS_FATAL("Image layout transition no support.");
        dstMask = 0;
        break;
    }

    barrier.srcAccessMask = srcMask;
    barrier.dstAccessMask = dstMask;

    vkCmdPipelineBarrier(
        cb,
        srcStageMask, dstStageMask,
        dependencyFlags,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void VulkanImage::clear(VkCommandBuffer cb,glm::vec4 colour)
{
    transitionLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearColorValue ccv{};
    ccv.float32[0] = colour.x;
    ccv.float32[1] = colour.y;
    ccv.float32[2] = colour.z;
    ccv.float32[3] = colour.w;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(
        cb,
        m_image,
        m_currentLayout, 
        &ccv,
        1, &range
    );
}

void VulkanImage::transitionLayoutImmediately(VkCommandPool pool,VkQueue queue,VkImageLayout newLayout,VkImageAspectFlags aspectFlags)
{
    executeImmediately(*m_device,pool,queue,[&](VkCommandBuffer cb)
    {
        transitionLayout(cb,newLayout,aspectFlags);
    });
}

void VulkanImage::upload(std::vector<uint8>& bytes,VkCommandPool pool,VkQueue queue,VkImageAspectFlagBits flag)
{
    VulkanBuffer* stageBuffer = VulkanBuffer::create(
        m_device,
        pool,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        (VkDeviceSize)bytes.size(),
        bytes.data()
    );

    executeImmediately(*m_device,pool,queue,[&](VkCommandBuffer cb)
    {
        clear(cb);
        setCurrentLayout(VK_IMAGE_LAYOUT_UNDEFINED);
        transitionLayout(cb,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,flag);
        copyBufferToImage(cb,*stageBuffer,m_createInfo.extent.width,m_createInfo.extent.height,flag);
        generateMipmaps(cb,m_createInfo.extent.width,m_createInfo.extent.height,m_createInfo.mipLevels,flag);
        transitionLayout(cb,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,flag);
    });

    delete stageBuffer;
}

void VulkanImage::generateMipmaps(VkCommandBuffer cb,int32_t texWidth,int32_t texHeight,uint32_t mipLevels,VkImageAspectFlagBits flag)
{
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_device->physicalDevice,m_createInfo.format,&formatProperties);

    if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        LOG_GRAPHICS_FATAL("Texel format no support linear lerp.");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = flag;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for(uint32_t i = 1; i<mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i-1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
            0,nullptr,
            0,nullptr,
            1,&barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = flag;
        blit.srcSubresource.mipLevel = i-1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth>1 ? mipWidth/2 : 1, mipHeight>1 ? mipHeight/2 : 1, 1};
        blit.dstSubresource.aspectMask = flag;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cb,
            m_image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,&blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
            0,nullptr,
            0,nullptr,
            1,&barrier);

        if(mipWidth>1) mipWidth /= 2;
        if(mipHeight>1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels-1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // 在这里设置当前的ImageLayout。
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
        0,nullptr,
        0,nullptr,
        1,&barrier);
}

void VulkanImage::copyBufferToImage(VkCommandBuffer cb,VkBuffer buffer,uint32 width,uint32 height,VkImageAspectFlagBits flag,uint32 mipmapLevel)
{
    transitionLayout(cb,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,flag);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = flag;
    region.imageSubresource.mipLevel = mipmapLevel;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };
    vkCmdCopyBufferToImage(
        cb,
        buffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
}


void VulkanImage::create(
    VulkanDevice* device,
    const VkImageCreateInfo& info, 
    VkImageViewType viewType,
    VkImageAspectFlags aspectMask,
    bool isHost,
    VmaMemoryUsage memUsage)
{
    this->m_device = device;
    this->m_createInfo = info;
    this->m_currentLayout = info.initialLayout;

    if (vkCreateImage(*m_device, &m_createInfo, nullptr, &m_image) != VK_SUCCESS) 
    {
        LOG_GRAPHICS_FATAL("Fail to create image handle.");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*m_device, m_image, &memRequirements);

    this->m_size = memRequirements.size;

    static const auto* cVarUseVma = CVarSystem::get()->getInt32CVar("r.RHI.EnableVma");
    if(*cVarUseVma != 0)
    {
        VmaAllocationCreateInfo vmaAllocInfo = {};

        if(isHost)
        {
            vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }
        else
        {
            vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        vmaAllocInfo.usage = memUsage;
        vkCheck(vmaAllocateMemory(VulkanRHI::get()->getVmaAllocator(),&memRequirements,&vmaAllocInfo,&m_allocation,nullptr));
        vmaBindImageMemory2(VulkanRHI::get()->getVmaAllocator(),m_allocation,0,m_image,nullptr);
    }
    else
    {
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        VkMemoryPropertyFlags properties{};
        if(isHost)
        {
            properties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
        else
        {
            properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(*m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) 
        {
            LOG_GRAPHICS_FATAL("Fail to allocate memory.");
        }

        vkBindImageMemory(*m_device, m_image, m_memory, 0);
    }

    if (!isHost) 
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = viewType;
        viewInfo.format = info.format;

        VkImageSubresourceRange subres{};
        subres.aspectMask = aspectMask;
        subres.baseArrayLayer = 0;
        subres.levelCount = info.mipLevels;
        subres.layerCount = info.arrayLayers;

        viewInfo.subresourceRange = subres;
        if (vkCreateImageView(*m_device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) 
        {
            LOG_GRAPHICS_FATAL("Fail to create image view.");
        }
    }
}

Texture2DImage* Texture2DImage::create( 
    VulkanDevice* device,
    uint32_t width, 
    uint32_t height,
    VkImageAspectFlags aspectMask,
    VkFormat format,
    bool isHost,
    int32 mipLevels)
{
    Texture2DImage* ret = new Texture2DImage();

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = {};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = mipLevels == -1 ? getMipLevelsCount(width,height) : mipLevels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = isHost ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = isHost ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
    ret->VulkanImage::create(device, info,VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, isHost);

    return ret;
}

Texture2DImage* Texture2DImage::createAndUpload(
    VulkanDevice* device,
    VkCommandPool pool,
    VkQueue queue,
    std::vector<uint8>& bytes,
    uint32_t width,
    uint32_t height,
    VkImageAspectFlags aspectMask,
    VkFormat format,
    VkImageAspectFlagBits flag,
    bool isHost,
    int32 mipLevels)
{
    Texture2DImage* ret = create(device,width,height,aspectMask,format,isHost,mipLevels);
    ret->upload(bytes,pool,queue,flag);
    return ret;
}

DepthStencilImage::~DepthStencilImage()
{
    if(m_depthOnlyImageView!=VK_NULL_HANDLE)
    {
        vkDestroyImageView(*m_device,m_depthOnlyImageView,nullptr);
    }
}

DepthStencilImage* DepthStencilImage::create(VulkanDevice* device,uint32_t width, uint32_t height,bool bCreateDepthOnlyView)
{
    DepthStencilImage* ret = new DepthStencilImage();
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = {};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = device->findDepthStencilFormat();
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ret->VulkanImage::create(device, info,viewType, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, false);

    if(bCreateDepthOnlyView)
    {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = ret->m_image;
		viewInfo.viewType = viewType;
		viewInfo.format = info.format;
		VkImageSubresourceRange subres{};
		subres.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		subres.baseArrayLayer = 0;
		subres.levelCount = info.mipLevels;
		subres.layerCount = info.arrayLayers;

		viewInfo.subresourceRange = subres;
		if(vkCreateImageView(*ret->m_device,&viewInfo,nullptr,&ret->m_depthOnlyImageView)!=VK_SUCCESS)
		{
			LOG_GRAPHICS_FATAL("Fail to create depth only image view.");
		}
    }

    return ret;
}

DepthOnlyImage::~DepthOnlyImage()
{
}

DepthOnlyImage* DepthOnlyImage::create(VulkanDevice* device,uint32_t width,uint32_t height)
{
    DepthOnlyImage* ret = new DepthOnlyImage();
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = {};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = device->findDepthOnlyFormat();
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ret->VulkanImage::create(device, info,viewType,  VK_IMAGE_ASPECT_DEPTH_BIT, false);

    return ret;
}

DepthOnlyTextureArray* DepthOnlyTextureArray::create(VulkanDevice* device,uint32_t width,uint32_t height,uint32 layerCount)
{
    DepthOnlyTextureArray* ret = new DepthOnlyTextureArray();

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = {};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = device->findDepthOnlyFormat();
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = layerCount; // Padding texture array layers here.
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // All layer views will be create here.
    ret->VulkanImage::create(device,info,VK_IMAGE_VIEW_TYPE_2D_ARRAY,VK_IMAGE_ASPECT_DEPTH_BIT,false);

    // create per texture view.
    std::vector<VkImageViewCreateInfo> viewsInfo;
    viewsInfo.resize(layerCount);
    for(uint32 index = 0; index < layerCount; index++)
    {
        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format = info.format;
        viewInfo.subresourceRange = {};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = index;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.image = ret->m_image;
        viewsInfo[index] = viewInfo;
    }
    ret->VulkanImageArray::InitViews(viewsInfo);

    return ret;
}

RenderTexture* RenderTexture::create(VulkanDevice* device,uint32_t width,uint32_t height,VkFormat format)
{
    RenderTexture* ret = new RenderTexture();

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = {};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ret->VulkanImage::create(device, info,VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, false);

    return ret;
}

void VulkanImageArray::InitViews(const std::vector<VkImageViewCreateInfo> infos)
{
    mPerTextureView.resize(infos.size());
    for(uint32 index = 0; index < infos.size(); index++)
    {
        mPerTextureView[index] = VK_NULL_HANDLE;
        vkCheck(vkCreateImageView(VulkanRHI::get()->getDevice(), &infos[index], nullptr, &mPerTextureView[index]));
    }
    
    bInitArrayViews = true;
}

}