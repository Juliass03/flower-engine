#pragma once
#include <vulkan/vulkan.h>
#include <Vma/vk_mem_alloc.h>
#include "vk_common.h"
#include "vk_device.h"

namespace engine {

class VulkanBuffer
{
	friend class VulkanVertexBuffer;
	friend class VulkanIndexBuffer;

private:
	VulkanDevice* m_device = nullptr;
	VkCommandPool m_commandPool = VK_NULL_HANDLE;
	bool m_isHeap = false;

	VkDescriptorBufferInfo m_descriptor = {};
	VkDeviceSize m_size = 0;
	VkDeviceSize m_alignment = 0;
	VkBufferUsageFlags m_usageFlags = {};
	VkMemoryPropertyFlags m_memoryPropertyFlags = {};
	VkBuffer m_buffer = VK_NULL_HANDLE;

	// m_allocation work when r.RHI.EnableVma is 1.
	VmaAllocation m_allocation = nullptr;

	// memory work when r.RHI.EnableVma is 0.
	VkDeviceMemory m_memory = VK_NULL_HANDLE;

private:
	VulkanBuffer() = default;
	void destroy();
	bool isHeap();
	bool CreateBuffer(
		VkBufferUsageFlags usageFlags, 
		const VmaMemoryUsage memoryUsage,
		VkMemoryPropertyFlags memoryPropertyFlags, 
		VkDeviceSize size, 
		void *data,
		bool bHeapMemory
	);

public:
	~VulkanBuffer() { destroy(); }
	void* mapped = nullptr;
	operator VkBuffer() { return m_buffer; }
	operator VkDeviceMemory() { return m_memory; }
	VkBuffer& GetVkBuffer() { return m_buffer; }

	static VulkanBuffer* create(
		VulkanDevice* in_device,
		VkCommandPool in_commandpool,
		VkBufferUsageFlags usageFlags, 
		const VmaMemoryUsage memoryUsage,
		VkMemoryPropertyFlags memoryPropertyFlags, 
		VkDeviceSize size, 
		void *data,
		bool bHeapMemory = false
	);

	VkDeviceSize getSize() { return m_size; }
	VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void unmap();
	VkResult bind(VkDeviceSize offset = 0);
	void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void copyTo(void* data, VkDeviceSize size);
	VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void stageCopyFrom(VulkanBuffer& inBuffer,VkDeviceSize size,VkQueue execute_queue);
};

class VulkanIndexBuffer
{
	VulkanIndexBuffer() = default;

public:
	~VulkanIndexBuffer() 
	{
		if(m_buffer != nullptr)
		{
			delete m_buffer;
		}
	}

	operator VkBuffer()
	{
		return *m_buffer;
	}

	VkBuffer getBuffer()
	{
		return *m_buffer;
	}

	VulkanBuffer* getVulkanBuffer() { return m_buffer; }

	inline void bind(VkCommandBuffer cmd_buf)
	{
		vkCmdBindIndexBuffer(cmd_buf, *m_buffer, 0, m_indexType);
	}

	inline void bindAndDraw(VkCommandBuffer cmd_buf)
	{
		vkCmdBindIndexBuffer(cmd_buf, *m_buffer, 0, m_indexType);
		vkCmdDrawIndexed(cmd_buf, m_indexCount, 1, 0, 0, 0);
	}

	static VulkanIndexBuffer* create(
		VulkanDevice* vulkanDevice,
		VkCommandPool pool,
		const std::vector<uint32_t>& indices
	);

	static VulkanIndexBuffer* create(
		VulkanDevice* vulkanDevice,
		VkDeviceSize bufferSize,
		VkCommandPool pool,
		bool bHeapMemory,
		bool bSSBO
	);

private:
	VulkanDevice* m_device = nullptr;

public:
	VulkanBuffer* m_buffer = nullptr;
	int32 m_indexCount = 0;
	VkIndexType	m_indexType = VK_INDEX_TYPE_UINT32;
};

class VulkanVertexBuffer
{
	VulkanVertexBuffer() = default;

public:
	~VulkanVertexBuffer() 
	{
		if(m_buffer!=nullptr)
		{
			delete m_buffer;
		}
	}

	operator VkBuffer()
	{
		return *m_buffer;
	}

	VkBuffer getBuffer()
	{
		return *m_buffer;
	}

	VulkanBuffer* getVulkanBuffer() { return m_buffer; }

	inline void bind(VkCommandBuffer cmdBuffer)
	{
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &(m_buffer->m_buffer), &m_offset);
	}

	VkVertexInputBindingDescription getInputBinding();
	std::vector<VkVertexInputAttributeDescription> getInputAttribute();

	static VkVertexInputBindingDescription getInputBinding(const std::vector<EVertexAttribute>& attributes);
	static std::vector<VkVertexInputAttributeDescription> getInputAttribute(const std::vector<EVertexAttribute>& attributes);

	static VulkanVertexBuffer* create(
		VulkanDevice* in_device,
		VkCommandPool pool,
		const std::vector<float>& vertices,
		const std::vector<EVertexAttribute>& attributes
	);

	static VulkanVertexBuffer* create(
		VulkanDevice* in_device,
		VkCommandPool pool,
		VkDeviceSize size,
		bool bHeapMemory,
		bool bSSBO
	);

private:
	VulkanDevice* m_device = nullptr;
	VulkanBuffer* m_buffer = nullptr;
	VkDeviceSize m_offset = 0;
	std::vector<EVertexAttribute> m_attributes;
};

}