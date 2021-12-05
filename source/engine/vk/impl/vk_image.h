#pragma once
#include "vk_device.h"
#include <array>
#include <Vma/vk_mem_alloc.h>
#include "../../core/core.h"

namespace engine{

    class VulkanImage
    {
    protected:
        VulkanDevice* m_device = nullptr;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VmaAllocation m_allocation = nullptr;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkDeviceSize m_size = {};
        VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageCreateInfo m_createInfo = {};
    public:
        VulkanImage() = default;

        virtual void create(
            VulkanDevice* device,
            const VkImageCreateInfo& info, 
            VkImageViewType viewType,
            VkImageAspectFlags aspectMask,
            bool isHost,
            VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY
        ); 

        void generateMipmaps(VkCommandBuffer cb,int32_t texWidth,int32_t texHeight,uint32_t mipLevels,VkImageAspectFlagBits flag);
        void copyBufferToImage(VkCommandBuffer cb,VkBuffer buffer,uint32 width,uint32 height,VkImageAspectFlagBits flag,uint32 mipmapLevel = 0);
        void setCurrentLayout(VkImageLayout oldLayout) { m_currentLayout = oldLayout; }
        void transitionLayout(VkCommandBuffer cb,VkImageLayout newLayout,VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    public:
        void transitionLayoutImmediately(VkCommandPool pool,VkQueue queue,VkImageLayout newLayout,VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
        virtual ~VulkanImage(){ release(); }
        VkImage getImage() const { return m_image; }
        VkImageView getImageView() const { return m_imageView; }
        VkDeviceMemory getMem() const { return m_memory; }
        VkFormat getFormat() const { return m_createInfo.format; }
        VkExtent3D getExtent() const { return m_createInfo.extent; }
        const VkImageCreateInfo& getInfo() const { return m_createInfo; }
        VkImageLayout getCurentLayout() const { return m_currentLayout; }
        void clear(VkCommandBuffer cb, glm::vec4 colour = {0, 0, 0, 0});
        void upload(std::vector<uint8>& bytes,VkCommandPool pool,VkQueue queue,VkImageAspectFlagBits flag = VK_IMAGE_ASPECT_COLOR_BIT);
        void release();

        // Tip: All image view create by these function manage by yourself, so make sure all resource release.
		void CreateRTV(VkImageView* pRV,int mipLevel = -1);
		void CreateSRV(VkImageView* pImageView,int mipLevel = -1);
		void CreateDSV(VkImageView* pView);
		void CreateCubeSRV(VkImageView* pImageView);
    };

    class VulkanImageArray: public VulkanImage
    {
    private:
        bool bInitArrayViews = false;

    protected:
        // 每层纹理的视图
        std::vector<VkImageView> mPerTextureView{};
        void release()
        {
            CHECK(bInitArrayViews);
            for(auto& view : mPerTextureView)
            {
                vkDestroyImageView(*m_device, view, nullptr);
            }
        }
    public:
        VkImageView getArrayImageView(uint32 layerIndex) const { return mPerTextureView[layerIndex]; }
        const std::vector<VkImageView>& getArrayImageViews() { return mPerTextureView; }

        void InitViews(const std::vector<VkImageViewCreateInfo> infos);

        virtual ~VulkanImageArray()
        {
            release();
        }
    };

    class Texture2DImage: public VulkanImage
    {
        Texture2DImage() = default;
    public:
        virtual ~Texture2DImage() = default;

        static Texture2DImage* create(
            VulkanDevice* device,
            uint32_t width, uint32_t height,
            VkImageAspectFlags aspectMask,
            VkFormat format,
            bool isHost = false,
            int32 mipLevels = -1
        );

        static Texture2DImage* createAndUpload(
            VulkanDevice* device,
            VkCommandPool pool,VkQueue queue,
            std::vector<uint8>& bytes,
            uint32_t width,uint32_t height,
            VkImageAspectFlags aspectMask,
            VkFormat format,
            VkImageAspectFlagBits flag = VK_IMAGE_ASPECT_COLOR_BIT,
            bool isHost = false,
            int32 mipLevels = -1
        );

        float getMipmapLevels() const
        {
            return (float)m_createInfo.mipLevels;
        }
    };

    class DepthStencilImage: public VulkanImage
    {
        DepthStencilImage() = default;
        VkImageView m_depthOnlyImageView = VK_NULL_HANDLE;
    public:
        virtual ~DepthStencilImage();
        VkImageView getDepthOnlyImageView() const { return m_depthOnlyImageView; }
        static DepthStencilImage* create(VulkanDevice* device,uint32_t width, uint32_t height,bool bCreateDepthOnlyView);
    };

    class DepthOnlyImage: public VulkanImage
    {
        DepthOnlyImage() = default;
    public:
        virtual ~DepthOnlyImage();
        static DepthOnlyImage* create(VulkanDevice* device,uint32_t width, uint32_t height);
    };

    class DepthOnlyTextureArray : public VulkanImageArray
    {
        DepthOnlyTextureArray() = default;
        
    public:
        static DepthOnlyTextureArray* create(VulkanDevice*device,uint32_t width,uint32_t height,uint32 layerCount);
    };

    class RenderTexture : public VulkanImage
    {
        RenderTexture() = default;
        bool bInitMipmapViews = false;
        std::vector<VkImageView> m_perMipmapTextureView{};

        void release()
        {
            for(auto& view : m_perMipmapTextureView)
            {
                CHECK(bInitMipmapViews);
                vkDestroyImageView(*m_device,view,nullptr);
            }
        }
    public:
        virtual ~RenderTexture()
        {
            release();
        }

        static RenderTexture* create(VulkanDevice*device,uint32_t width,uint32_t height,VkFormat format,VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        static RenderTexture* create(VulkanDevice*device,uint32_t width,uint32_t height,uint32_t mipmapCount,bool bCreateMipmaps,VkFormat format,VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
       
        VkImageView getMipmapView(uint32 level);
    };

    class RenderTextureCube : public VulkanImage
    {
        RenderTextureCube() = default;
        bool bInitMipmapViews = false;
		std::vector<VkImageView> m_perMipmapTextureView{};
		void release()
		{
			for(auto& view : m_perMipmapTextureView)
			{
                CHECK(bInitMipmapViews);
				vkDestroyImageView(*m_device,view,nullptr);
			}
		}
    public:
        virtual ~RenderTextureCube() 
        {
            release();
        }
        static RenderTextureCube* create(
            VulkanDevice*device,
            uint32_t width,
            uint32_t height,
            uint32_t mipmapLevels,
            VkFormat format,
            VkImageUsageFlags usage = 
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
                VK_IMAGE_USAGE_SAMPLED_BIT,
            bool bCreateViewsForMips = false
         );

        VkImageView getMipmapView(uint32 level);
    };

}