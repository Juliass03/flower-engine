#pragma once
#include "vk_device.h"

namespace engine{

class VulkanSampler
{
    friend class SamplerFactory;
public:
    ~VulkanSampler()
    {
        if (m_imageSampler != VK_NULL_HANDLE) 
        {
            vkDestroySampler(*m_device, m_imageSampler, nullptr);
            m_imageSampler = VK_NULL_HANDLE;
        }
    }

    operator VkSampler()
    {
        return m_imageSampler;
    }

    VkSampler& get()
    {
        return m_imageSampler;
    }

private:
    VulkanSampler(VulkanDevice* device,const VkSamplerCreateInfo& info)
    {
        m_device = device;
        vkCheck(vkCreateSampler(*device, &info, nullptr, &m_imageSampler));
    }

    VkSampler m_imageSampler;
    VulkanDevice* m_device;
};

class SamplerFactory
{
private:
    struct State 
    {
        VkSamplerCreateInfo info;
    };
    State s;

public:
    SamplerFactory() 
    {
        s.info = {};
        s.info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        s.info.pNext = nullptr;
        s.info.magFilter = VK_FILTER_NEAREST;
        s.info.minFilter = VK_FILTER_NEAREST;
        s.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        s.info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        s.info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        s.info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        s.info.mipLodBias = 0.0f;
        s.info.anisotropyEnable = 0;
        s.info.maxAnisotropy = 0.0f;
        s.info.compareEnable = 0;
        s.info.compareOp = VK_COMPARE_OP_NEVER;
        s.info.minLod = 0;
        s.info.maxLod = 0;
        s.info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        s.info.unnormalizedCoordinates = 0;
    }

    SamplerFactory& Flags(VkSamplerCreateFlags value)
    {
        s.info.flags = value; return *this;
    }
    SamplerFactory& MagFilter(VkFilter value)
    {
        s.info.magFilter = value; return *this;
    }
    SamplerFactory& MinFilter(VkFilter value)
    {
        s.info.minFilter = value; return *this;
    }
    SamplerFactory& MipmapMode(VkSamplerMipmapMode value)
    {
        s.info.mipmapMode = value; return *this;
    }
    SamplerFactory& AddressModeU(VkSamplerAddressMode value)
    {
        s.info.addressModeU = value; return *this;
    }
    SamplerFactory& AddressModeV(VkSamplerAddressMode value)
    {
        s.info.addressModeV = value; return *this;
    }
    SamplerFactory& AddressModeW(VkSamplerAddressMode value)
    {
        s.info.addressModeW = value; return *this;
    }
    SamplerFactory& MipLodBias(float value)
    {
        s.info.mipLodBias = value; return *this;
    }
    SamplerFactory& AnisotropyEnable(VkBool32 value)
    {
        s.info.anisotropyEnable = value; return *this;
    }
    SamplerFactory& MaxAnisotropy(float value)
    {
        s.info.maxAnisotropy = value; return *this;
    }
    SamplerFactory& CompareEnable(VkBool32 value)
    {
        s.info.compareEnable = value; return *this;
    }
    SamplerFactory& CompareOp(VkCompareOp value)
    {
        s.info.compareOp = value; return *this;
    }
    SamplerFactory& MinLod(float value)
    {
        s.info.minLod = value; return *this;
    }
    SamplerFactory& MaxLod(float value)
    {
        s.info.maxLod = value; return *this;
    }
    SamplerFactory& BorderColor(VkBorderColor value)
    {
        s.info.borderColor = value; return *this;
    }
    SamplerFactory& UnnormalizedCoordinates(VkBool32 value)
    {
        s.info.unnormalizedCoordinates = value; return *this;
    }

    VulkanSampler* create(VulkanDevice* device)
    {
        return new VulkanSampler(device,s.info);
    }
};

}