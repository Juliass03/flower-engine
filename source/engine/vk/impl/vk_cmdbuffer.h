#pragma once
#include "vk_device.h"
#include <memory>
#include <vector>

namespace engine{

class VulkanCommandBuffer
{
private:
	VkCommandBuffer m_commandBuffer;
	VkCommandPool m_pool;
	VulkanDevice* m_device;
	VkQueue m_queue;
	bool m_begin;
	VulkanCommandBuffer() = default;

public:
	~VulkanCommandBuffer();
	operator VkCommandBuffer(){ return m_commandBuffer; }
	auto& getInstance() { return m_commandBuffer; }
	auto& getQueue() { return m_queue; }

public:
	void begin(VkCommandBufferUsageFlagBits flag = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	void end();
	void flush();

	static VulkanCommandBuffer* create(
		VulkanDevice* in_device, 
		VkCommandPool command_pool, 
		VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
		VkQueue queue = VK_NULL_HANDLE
	);
};

}