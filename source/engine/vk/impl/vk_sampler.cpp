#include "vk_sampler.h"
#include "../vk_rhi.h"
#include <sstream>
#include <string>
#include <functional>
#include "../../core/crc.h"

namespace engine{

void VulkanSamplerCache::init(VulkanDevice* newDevice)
{
	m_device = newDevice;
}

void VulkanSamplerCache::cleanup()
{
    for(auto pair : m_samplerCache)
    {
        vkDestroySampler(*m_device, pair.second, nullptr);
    }
    m_samplerCache.clear();
}

VkSampler VulkanSamplerCache::createSampler(VkSamplerCreateInfo* info)
{
    SamplerCreateInfo sci{};
    sci.info = *info;
    
    auto it = m_samplerCache.find(sci);
    if(it != m_samplerCache.end())
    {
        // 若已存在Cache中则直接返回
        return (*it).second;
    }
    else
    {
        // 否则新建Sampler并存在Cache中
        VkSampler sampler;
        vkCheck(vkCreateSampler(*m_device, &sci.info, nullptr, &sampler));
        m_samplerCache[sci] = sampler;
        return sampler;
    }
}

bool VulkanSamplerCache::SamplerCreateInfo::operator==(const SamplerCreateInfo& other) const
{
    return  (other.info.addressModeU == this->info.addressModeU) && 
            (other.info.addressModeV == this->info.addressModeV) && 
            (other.info.addressModeW == this->info.addressModeW) && 
            (other.info.anisotropyEnable == this->info.anisotropyEnable) && 
            (other.info.borderColor == this->info.borderColor) && 
            (other.info.compareEnable == this->info.compareEnable) && 
            (other.info.compareOp == this->info.compareOp) && 
            (other.info.flags == this->info.flags) && 
            (other.info.magFilter == this->info.magFilter) && 
            (other.info.maxAnisotropy == this->info.maxAnisotropy) && 
            (other.info.maxLod == this->info.maxLod) && 
            (other.info.minFilter == this->info.minFilter) && 
            (other.info.minLod == this->info.minLod) && 
            (other.info.mipmapMode == this->info.mipmapMode) && 
            (other.info.unnormalizedCoordinates == this->info.unnormalizedCoordinates);
}

size_t VulkanSamplerCache::SamplerCreateInfo::hash() const
{
    return Crc::memCrc32(&this->info,sizeof(VkSamplerCreateInfo));
}

}