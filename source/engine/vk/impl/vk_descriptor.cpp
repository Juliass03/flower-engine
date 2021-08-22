#include "vk_descriptor.h"
#include <algorithm>

namespace engine{

// 根据预设好的PoolSizes创建描述符池
VkDescriptorPool createPool(VkDevice m_device,const VulkanDescriptorAllocator::PoolSizes& poolSizes,int count,VkDescriptorPoolCreateFlags flags)
{
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(poolSizes.sizes.size());
    for(auto sz:poolSizes.sizes)
    {
        // count为要申请的池子数目
        // 每种DescriptorType的VkDescriptorPoolSize都不同，按照预设来申请
        sizes.push_back({ sz.first, uint32_t( sz.second * count)});
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = count;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(m_device,&pool_info,nullptr,&descriptorPool);
    return descriptorPool;
}

void VulkanDescriptorAllocator::resetPools()
{
    for(auto p: m_usedPools)
    {
        vkResetDescriptorPool(*m_device,p,0);
    }

    m_freePools = m_usedPools;
    m_usedPools.clear();
    m_currentPool = VK_NULL_HANDLE;
}

bool VulkanDescriptorAllocator::allocate(VkDescriptorSet* set,VkDescriptorSetLayout layout)
{
    // 若当前工作中的描述符池为空则申请一个
    if(m_currentPool == VK_NULL_HANDLE)
    {
        m_currentPool = requestPool();

        // 新申请的描述符池将作为工作符池
        m_usedPools.push_back(m_currentPool);
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = m_currentPool;
    allocInfo.descriptorSetCount = 1;
    VkResult allocResult = vkAllocateDescriptorSets(*m_device,&allocInfo,set);
    bool needReallocate = false;

    switch(allocResult)
    {
    case VK_SUCCESS:
        return true;
        break;

        // 需要重新申请
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        needReallocate = true;
        break;
    default:
        return false;
    }

    if(needReallocate)
    {
        // 重新申请描述符池
        m_currentPool = requestPool();
        m_usedPools.push_back(m_currentPool);
        allocResult = vkAllocateDescriptorSets(*m_device,&allocInfo,set);
        if(allocResult == VK_SUCCESS)
        {
            return true;
        }
    }

    return false;
}

void VulkanDescriptorAllocator::init(VulkanDevice* newDevice)
{
    m_device = newDevice;
}

void VulkanDescriptorAllocator::cleanup()
{
    resetPools();
    for(auto p : m_freePools)
    {
        vkDestroyDescriptorPool(*m_device,p,nullptr);
    }
    for(auto p : m_usedPools)
    {
        vkDestroyDescriptorPool(*m_device,p,nullptr);
    }
}

VkDescriptorPool VulkanDescriptorAllocator::requestPool()
{
    if(m_freePools.size()>0)
    {
        VkDescriptorPool pool = m_freePools.back();
        m_freePools.pop_back();
        return pool;
    }
    else
    {
        // 若空闲的池数目不够多则再次申请。
        // 不需要VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT标志位
        return createPool(*m_device,m_descriptorSizes,1000,0); 
    }
}


void VulkanDescriptorLayoutCache::init(VulkanDevice* newDevice)
{
    m_device = newDevice;
}

VkDescriptorSetLayout VulkanDescriptorLayoutCache::createDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info)
{
    DescriptorLayoutInfo layoutinfo;
    layoutinfo.bindings.reserve(info->bindingCount);
    bool isSorted = true;
    int32_t lastBinding = -1;

    // 填充DescriptorLayoutInfo
    for(uint32_t i = 0; i < info->bindingCount; i++)
    {
        layoutinfo.bindings.push_back(info->pBindings[i]);
        if(static_cast<int32_t>(info->pBindings[i].binding) > lastBinding)
        {
            lastBinding = info->pBindings[i].binding;
        }
        else
        {
            isSorted = false;
        }
    }

    // 排序
    if(!isSorted)
    {
        std::sort(layoutinfo.bindings.begin(),layoutinfo.bindings.end(),[](VkDescriptorSetLayoutBinding& a,VkDescriptorSetLayoutBinding& b)
        {
            return a.binding < b.binding;
        });
    }

    // 查找已有的LayoutCache是否已经创建
    auto it = m_layoutCache.find(layoutinfo);
    if(it != m_layoutCache.end())
    {
        // 若已存在Cache中则直接返回
        return (*it).second;
    }
    else
    {
        // 否则新建VkDescriptorSetLayout并存在Cache中
        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(*m_device,info,nullptr,&layout);
        m_layoutCache[layoutinfo] = layout;
        return layout;
    }
}


void VulkanDescriptorLayoutCache::cleanup()
{
    for(auto pair : m_layoutCache)
    {
        vkDestroyDescriptorSetLayout(*m_device,pair.second,nullptr);
    }
    m_layoutCache.clear();
}

VulkanDescriptorFactory VulkanDescriptorFactory::begin(VulkanDescriptorLayoutCache* layoutCache,VulkanDescriptorAllocator* allocator)
{
    VulkanDescriptorFactory builder;
    builder.m_cache = layoutCache;
    builder.m_allocator = allocator;
    return builder;
}

VulkanDescriptorFactory& VulkanDescriptorFactory::bindBuffer(uint32_t binding,VkDescriptorBufferInfo* bufferInfo,VkDescriptorType type,VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding newBinding{};
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;
    m_bindings.push_back(newBinding);

    DescriptorWriteContainer descriptorWrite{};
    descriptorWrite.isImg = false;
    descriptorWrite.bufInfo = *bufferInfo;
    descriptorWrite.type = type;
    descriptorWrite.binding = binding;
    m_descriptorWriteBufInfos.push_back(descriptorWrite);

    return *this;
}

VulkanDescriptorFactory& VulkanDescriptorFactory::bindImage(
    uint32_t binding,
    VkDescriptorImageInfo* info,
    VkDescriptorType type,
    VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    m_bindings.push_back(newBinding);

    DescriptorWriteContainer descriptorWrite{};
    descriptorWrite.isImg = true;
    descriptorWrite.imgInfo = *info;
    descriptorWrite.type = type;
    descriptorWrite.binding = binding;
    m_descriptorWriteBufInfos.push_back(descriptorWrite);

    return *this;
}

bool VulkanDescriptorFactory::build(VulkanDescriptorSetReference& set,VulkanDescriptorLayoutReference& layout)
{
    // 构建VkDescriptorSetLayout并保存到Cache中
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pBindings = m_bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
    layout.layout = m_cache->createDescriptorLayout(&layoutInfo);
    bool success = m_allocator->allocate(&set.set,layout.layout);

    if(!success)
    {
        return false;
    };

    std::vector<VkWriteDescriptorSet> writes{};
    for(auto& dc : m_descriptorWriteBufInfos)
    {
        VkWriteDescriptorSet newWrite{};
        newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newWrite.pNext = nullptr;
        newWrite.descriptorCount = 1;

        newWrite.descriptorType = dc.type;

        if(dc.isImg)
            newWrite.pImageInfo = &dc.imgInfo;
        else
            newWrite.pBufferInfo = &dc.bufInfo;

        newWrite.dstBinding = dc.binding;
        newWrite.dstSet = set.set;
        writes.push_back(newWrite);
    }

    vkUpdateDescriptorSets(*m_allocator->m_device,static_cast<uint32_t>(writes.size()),writes.data(),0,nullptr);
    return true;
}

bool VulkanDescriptorFactory::build(VulkanDescriptorSetReference& set)
{
    // layout将会在Cache中缓存并统一管理生命周期
    VulkanDescriptorLayoutReference layout;
    return build(set,layout);
}

bool VulkanDescriptorFactory::build(VkDescriptorSet* set)
{
    VulkanDescriptorLayoutReference layout;
    // 构建VkDescriptorSetLayout并保存到Cache中
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pBindings = m_bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
    layout.layout = m_cache->createDescriptorLayout(&layoutInfo);
    bool success = m_allocator->allocate(set,layout.layout);

    if(!success)
    {
        return false;
    };

    std::vector<VkWriteDescriptorSet> writes{};
    for(auto& dc : m_descriptorWriteBufInfos)
    {
        VkWriteDescriptorSet newWrite{};
        newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newWrite.pNext = nullptr;
        newWrite.descriptorCount = 1;
        newWrite.descriptorType = dc.type;

        if(dc.isImg)
        {
            newWrite.pImageInfo = &dc.imgInfo;
        }
        else
        {
            newWrite.pBufferInfo = &dc.bufInfo;
        }

        newWrite.dstBinding = dc.binding;
        newWrite.dstSet = *set;
        writes.push_back(newWrite);
    }

    vkUpdateDescriptorSets(*m_allocator->m_device,static_cast<uint32_t>(writes.size()),writes.data(),0,nullptr);
    return true;
}

// 重载的等号运算符，用于Cache中的哈希查找
bool VulkanDescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
    if(other.bindings.size() != bindings.size())
    {
        // binding数目不同可以直接返回
        return false;
    }
    else
    {
        // 比较每一项是否相同
        for(int i = 0; i<bindings.size(); i++)
        {
            if(other.bindings[i].binding != bindings[i].binding)
            {
                return false;
            }
            if(other.bindings[i].descriptorType != bindings[i].descriptorType)
            {
                return false;
            }
            if(other.bindings[i].descriptorCount!=bindings[i].descriptorCount)
            {
                return false;
            }
            if(other.bindings[i].stageFlags!=bindings[i].stageFlags)
            {
                return false;
            }
        }
        return true;
    }
}

size_t VulkanDescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
    using std::size_t;
    using std::hash;

    size_t result = hash<size_t>()(bindings.size());

    for(const VkDescriptorSetLayoutBinding& b:bindings)
    {
        size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
        result ^= hash<size_t>()(binding_hash);
    }

    return result;
}

}