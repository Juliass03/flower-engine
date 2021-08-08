#include "vk_cmdbuffer.h"

namespace engine{

VulkanCommandBuffer::~VulkanCommandBuffer()
{
	if(m_commandBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(*m_device,m_pool,1,&m_commandBuffer);
		m_commandBuffer = VK_NULL_HANDLE;
	}
}

void VulkanCommandBuffer::begin(VkCommandBufferUsageFlagBits flag)
{
	if(m_begin)
	{
		return;
	}
	m_begin = true;

	VkCommandBufferBeginInfo command_buffer_begin_info{ };
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.flags = flag;

	if(vkBeginCommandBuffer(m_commandBuffer,&command_buffer_begin_info) != VK_SUCCESS)
	{
		LOG_GRAPHICS_FATAL("Fail to begin vulkan CommandBuffer.");
	}
}

void VulkanCommandBuffer::end()
{
	if(!m_begin)
	{
		return;
	}

	m_begin = false;
	if(vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
	{
		LOG_GRAPHICS_FATAL("Fail to end Vulkan CommandBuffer.");
	}
}

void VulkanCommandBuffer::flush()
{
	end();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffer;

	vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_queue);
}

VulkanCommandBuffer* VulkanCommandBuffer::create(
	VulkanDevice* in_device,
	VkCommandPool command_pool,
	VkCommandBufferLevel level,
	VkQueue queue)
{
	auto ret_command_buffer = new VulkanCommandBuffer(); 
	ret_command_buffer->m_device = in_device;
	ret_command_buffer->m_pool = command_pool;
	ret_command_buffer->m_begin = false;

	if(queue != VK_NULL_HANDLE)
	{
		ret_command_buffer->m_queue = queue;
	}
	else
	{
		ret_command_buffer->m_queue = in_device->graphicsQueue;
	}

	VkCommandBufferAllocateInfo cmdBufferAllocateInfo {};
	cmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocateInfo.commandPool = ret_command_buffer->m_pool;
	cmdBufferAllocateInfo.level = level;
	cmdBufferAllocateInfo.commandBufferCount = 1;

	if(vkAllocateCommandBuffers(*ret_command_buffer->m_device,&cmdBufferAllocateInfo,&(ret_command_buffer->m_commandBuffer)) != VK_SUCCESS)
	{
		LOG_GRAPHICS_FATAL("Fail to allocate vulkan CommandBuffer.");
	}

	return ret_command_buffer;
}

}