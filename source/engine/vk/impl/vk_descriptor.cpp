#include "vk_descriptor.h"
#include <algorithm>

namespace engine{

// ����Ԥ��õ�PoolSizes������������
VkDescriptorPool createPool(VkDevice m_device,const VulkanDescriptorAllocator::PoolSizes& poolSizes,int count,VkDescriptorPoolCreateFlags flags)
{
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(poolSizes.sizes.size());
    for(auto sz:poolSizes.sizes)
    {
        // countΪҪ����ĳ�����Ŀ
        // ÿ��DescriptorType��VkDescriptorPoolSize����ͬ������Ԥ��������
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
    // ����ǰ�����е���������Ϊ��������һ��
    if(m_currentPool == VK_NULL_HANDLE)
    {
        m_currentPool = requestPool();

        // ��������������ؽ���Ϊ��������
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

        // ��Ҫ��������
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        needReallocate = true;
        break;
    default:
        return false;
    }

    if(needReallocate)
    {
        // ����������������
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
        // �����еĳ���Ŀ���������ٴ����롣
        // ����ҪVK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT��־λ
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

    // ���DescriptorLayoutInfo
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

    // ����
    if(!isSorted)
    {
        std::sort(layoutinfo.bindings.begin(),layoutinfo.bindings.end(),[](VkDescriptorSetLayoutBinding& a,VkDescriptorSetLayoutBinding& b)
        {
            return a.binding < b.binding;
        });
    }

    // �������е�LayoutCache�Ƿ��Ѿ�����
    auto it = m_layoutCache.find(layoutinfo);
    if(it != m_layoutCache.end())
    {
        // ���Ѵ���Cache����ֱ�ӷ���
        return (*it).second;
    }
    else
    {
        // �����½�VkDescriptorSetLayout������Cache��
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
    // ����VkDescriptorSetLayout�����浽Cache��
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
    // layout������Cache�л��沢ͳһ������������
    VulkanDescriptorLayoutReference layout;
    return build(set,layout);
}

bool VulkanDescriptorFactory::build(VkDescriptorSet* set)
{
    VulkanDescriptorLayoutReference layout;
    // ����VkDescriptorSetLayout�����浽Cache��
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

// ���صĵȺ������������Cache�еĹ�ϣ����
bool VulkanDescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
    if(other.bindings.size() != bindings.size())
    {
        // binding��Ŀ��ͬ����ֱ�ӷ���
        return false;
    }
    else
    {
        // �Ƚ�ÿһ���Ƿ���ͬ
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