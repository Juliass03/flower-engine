#pragma once
#include "vk_common.h"

namespace engine{

class VulkanFencePool;
class VulkanFence
{
	friend VulkanFencePool;
public:
	enum class State
	{
		Signaled,
		UnSignaled,
	};
private:
	VkFence m_fence;
	State m_state;
	Ref<VulkanFencePool> m_pool;

public:
	Ref<VulkanFencePool> getPool() { return m_pool; }
	inline bool signaled() const { return m_state == State::Signaled; }
	inline VkFence getFence() const { return m_fence; }
	VulkanFence(VkDevice device,Ref<VulkanFencePool> pool,bool setSignaledOnCreated = false);
	~VulkanFence();

};

class VulkanFencePool
{
private:
	VkDevice m_device;
	std::vector<VulkanFence*> m_freeFences;
	std::vector<VulkanFence*> m_busyFences;

private:
	void destroyFence(VulkanFence* fence);
	bool checkFenceState(VulkanFence* fence);

public:
	VulkanFencePool();
	~VulkanFencePool();
	bool isFenceSignaled(VulkanFence* fence);
	void init(VkDevice device);
	void release();
	Ref<VulkanFence> createFence(bool signaledOnCreate);
	bool waitForFence(VulkanFence* fence, uint64 timeInNanoseconds = 1000000);
	void resetFence(VulkanFence* fence);

	// NOTE: «ø÷∆ Õ∑≈Fence
	void releaseFence(VulkanFence*& fence);

	void waitAndReleaseFence(VulkanFence*& fence, uint64 timeInNanoseconds = 1000000);
};

}