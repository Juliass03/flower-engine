#include "vk_fence.h"

using namespace engine;

engine::VulkanFence::VulkanFence(VkDevice device,Ref<VulkanFencePool> pool,bool setSignaledOnCreated)
: m_fence(VK_NULL_HANDLE),
  m_state(setSignaledOnCreated ? State::Signaled : State::UnSignaled),
  m_pool(pool)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	createInfo.flags = setSignaledOnCreated ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	vkCreateFence(device, &createInfo, nullptr, &m_fence);
}

engine::VulkanFence::~VulkanFence()
{
	if(m_fence!=VK_NULL_HANDLE)
	{
		LOG_WARN("Unrelease fence!");
	}
}

void engine::VulkanFencePool::destroyFence(VulkanFence* fence)
{
	vkDestroyFence(m_device, fence->m_fence, nullptr);
	fence->m_fence = VK_NULL_HANDLE;
	delete fence;
}

bool engine::VulkanFencePool::checkFenceState(VulkanFence* fence)
{
	VkResult result = vkGetFenceStatus(m_device, fence->m_fence);
	switch (result)
	{
	case VK_SUCCESS:
		fence->m_state = VulkanFence::State::Signaled;
		break;
	case VK_NOT_READY:
		break;
	default:
		break;
	}
	return false;
}

engine::VulkanFencePool::VulkanFencePool()
: m_device(VK_NULL_HANDLE)
{
}

engine::VulkanFencePool::~VulkanFencePool()
{
	if(m_busyFences.size()!=0)
	{
		LOG_WARN("No all busy fences finish!");
	}
}

bool engine::VulkanFencePool::isFenceSignaled(VulkanFence* fence)
{
	if (fence->signaled()) 
	{
		return true;
	}
	return checkFenceState(fence);
}

void engine::VulkanFencePool::init(VkDevice device)
{
	m_device = device;
}

void engine::VulkanFencePool::release()
{
	vkDeviceWaitIdle(m_device);
	CHECK(m_busyFences.size() <= 0);

	for (int32 i = 0; i < m_freeFences.size(); ++i) 
	{
		destroyFence(m_freeFences[i]);
	}
}

Ref<VulkanFence> engine::VulkanFencePool::createFence(bool signaledOnCreate)
{
	if (m_freeFences.size() > 0)
	{
		Ref<VulkanFence> fence = m_freeFences.back();
		m_freeFences.pop_back();
		m_busyFences.push_back(fence);
		if (signaledOnCreate) 
		{
			fence->m_state = VulkanFence::State::Signaled;
		}
		return fence;
	}

	VulkanFence* newFence = new VulkanFence(m_device, this, signaledOnCreate);
	m_busyFences.push_back(newFence);
	return newFence;
}

bool engine::VulkanFencePool::waitForFence(VulkanFence* fence,uint64 timeInNanoseconds)
{
	VkResult result = vkWaitForFences(m_device, 1, &fence->m_fence, true, timeInNanoseconds);
	switch (result)
	{
	case VK_SUCCESS:
		fence->m_state = VulkanFence::State::Signaled;
		return true;
	case VK_TIMEOUT:
		return false;
	default:
		LOG_WARN("Unkow error {0}!", (int32)result);
		return false;
	}
}

void engine::VulkanFencePool::resetFence(VulkanFence* fence)
{
	if (fence->m_state != VulkanFence::State::UnSignaled)
	{
		vkResetFences(m_device, 1, &fence->m_fence);
		fence->m_state = VulkanFence::State::UnSignaled;
	}
}

void engine::VulkanFencePool::releaseFence(VulkanFence*& fence)
{
	resetFence(fence);
	for (int32 i = 0; i < m_busyFences.size(); ++i) 
	{
		if (m_busyFences[i] == fence)
		{
			m_busyFences.erase(m_busyFences.begin() + i);
			break;
		}
	}
	m_freeFences.push_back(fence);
	fence = nullptr;
}

void engine::VulkanFencePool::waitAndReleaseFence(VulkanFence*& fence,uint64 timeInNanoseconds)
{
	if (!fence->signaled()) 
	{
		waitForFence(fence, timeInNanoseconds);
	}
	releaseFence(fence);
}
