#define VMA_IMPLEMENTATION
#include <Vma/vk_mem_alloc.h>
#include "vk_buffer.h"
#include "vk_cmdbuffer.h"
#include "../vk_rhi.h"

namespace engine{

static AutoCVarInt32 cVarEnableVma(
	"r.RHI.EnableVma",
	"Enable vma allocator to manage vkBuffer create and destroy. 0 is off, others are on.",
	"RHI",
	1, // 申请超大的VRAM时( > 256),VMA在低端机器上会有报错
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

VulkanBuffer* VulkanBuffer::create(
	VulkanDevice* in_device,
	VkCommandPool in_commandpool,
	VkBufferUsageFlags usageFlags, 
	const VmaMemoryUsage memoryUsage,
	VkMemoryPropertyFlags memoryPropertyFlags, 
	VkDeviceSize size, 
	void *data,
	bool bHeapMemory)
{
	auto ret_buffer = new VulkanBuffer();

	ret_buffer->m_device = in_device;
	ret_buffer->m_commandPool = in_commandpool;

	ret_buffer->CreateBuffer(usageFlags,memoryUsage,memoryPropertyFlags, size,data,bHeapMemory);
	ret_buffer->setupDescriptor();

	ret_buffer->m_size = size;

	return ret_buffer;
}

VkResult VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
	VkResult res;

	if(cVarEnableVma.get())
	{
		res = vmaMapMemory(VulkanRHI::get()->getVmaAllocator(),m_allocation,&mapped);
	}
	else
	{
		res = vkMapMemory(*m_device, m_memory, offset, size, 0, &mapped);
	}

	return res; 
}

void VulkanBuffer::unmap()
{
	if (mapped)
	{
		if(cVarEnableVma.get())
		{
			vmaUnmapMemory(VulkanRHI::get()->getVmaAllocator(),m_allocation);
			mapped = nullptr;
		}
		else
		{
			vkUnmapMemory(*m_device, m_memory);
			mapped = nullptr;
		}
	}
}

VkResult VulkanBuffer::bind(VkDeviceSize offset)
{
	if(cVarEnableVma.get())
	{
		return vmaBindBufferMemory2(VulkanRHI::get()->getVmaAllocator(), m_allocation,offset,m_buffer,nullptr);
	}
	else
	{
		return vkBindBufferMemory(*m_device, m_buffer, m_memory, offset);
	}
}

void VulkanBuffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
	m_descriptor.offset = offset;
	m_descriptor.buffer = m_buffer;
	m_descriptor.range = size;
}

void VulkanBuffer::copyTo(void* data, VkDeviceSize size)
{
	assert(mapped);
	memcpy(mapped, data, size);
}

VkResult VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset)
{
	VkResult res;
	if(cVarEnableVma.get())
	{
		res = vmaFlushAllocation(VulkanRHI::get()->getVmaAllocator(),m_allocation,offset,size);
	}
	else
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = m_memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		res = vkFlushMappedMemoryRanges(*m_device, 1, &mappedRange);
	}

	return res;
}

VkResult VulkanBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
{
	VkResult res;
	if(cVarEnableVma.get())
	{
		res = vmaInvalidateAllocation(VulkanRHI::get()->getVmaAllocator(),m_allocation,offset,size);
	}
	else
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = m_memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		res = vkInvalidateMappedMemoryRanges(*m_device, 1, &mappedRange);
	}

	return res;
}

bool VulkanBuffer::CreateBuffer(
	VkBufferUsageFlags usageFlags,
	const VmaMemoryUsage memoryUsage,
	VkMemoryPropertyFlags memoryPropertyFlags,
	VkDeviceSize size,void* data,bool bHeapMemory) 
{
	m_isHeap = bHeapMemory;

	//1. 创建 buffer 句柄
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if(!isHeap())
	{
		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		vkCheck(vmaCreateBuffer(VulkanRHI::get()->getVmaAllocator(), &bufferInfo, &vmaallocInfo,
			&m_buffer,
			&m_allocation,
			nullptr));

		if (data != nullptr)
		{
			void* mapped;
			vmaMapMemory(VulkanRHI::get()->getVmaAllocator(), m_allocation, &mapped); // 内存映射
			memcpy(mapped, data, size); // 内存复制
			vmaUnmapMemory(VulkanRHI::get()->getVmaAllocator(), m_allocation);// 内存取消映射
		}

		// vma already bind buffer memory.
		// vmaBindBufferMemory(VulkanRHI::get()->GetVmaAllocator(),m_allocation,buffer);
	}
	else
	{
		if (vkCreateBuffer(*m_device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) 
		{
			LOG_GRAPHICS_FATAL("Fail to create vulkan buffer.");
		}

		// 2. 分配内存
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(*m_device, m_buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

		if (vkAllocateMemory(*m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) 
		{
			LOG_GRAPHICS_FATAL("Fail to allocate vulkan buffer.");
		}

		if (data != nullptr)
		{
			void *mapped;
			vkMapMemory(*m_device, m_memory, 0, size, 0, &mapped); // 内存映射
			memcpy(mapped, data, size); // 内存复制
			vkUnmapMemory(*m_device,m_memory);// 内存取消映射
		}

		// 内存绑定到句柄中
		vkBindBufferMemory(*m_device, m_buffer, m_memory, 0);
	}
	return true;
}


void VulkanBuffer::stageCopyFrom(VulkanBuffer& inBuffer,VkDeviceSize size,VkQueue execute_queue, VkDeviceSize srcOffset, VkDeviceSize destOffset)
{
	auto cmd_buf = VulkanCommandBuffer::create(m_device,m_commandPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY,execute_queue);
	cmd_buf->begin();
	VkBufferCopy copyRegion{};

	copyRegion.srcOffset = srcOffset; 
	copyRegion.dstOffset = destOffset; 
	copyRegion.size = size;
	vkCmdCopyBuffer(*cmd_buf, inBuffer.m_buffer, this->m_buffer, 1, &copyRegion);

	cmd_buf->flush();

	delete cmd_buf;
}

void VulkanBuffer::destroy()
{
	if(!isHeap())
	{
		vmaDestroyBuffer(VulkanRHI::get()->getVmaAllocator(), m_buffer, m_allocation);
	}
	else
	{
		if (m_buffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(*m_device, m_buffer, nullptr);
		}
		if (m_memory != VK_NULL_HANDLE)
		{
			vkFreeMemory(*m_device, m_memory, nullptr);
		}
	}
}

bool VulkanBuffer::isHeap()
{
	return (cVarEnableVma.get() == 0) || m_isHeap;
}

VulkanIndexBuffer* VulkanIndexBuffer::create(
	VulkanDevice* in_device,
	VkCommandPool pool,
	const std::vector<uint32_t>& indices)
{
	VulkanIndexBuffer* ret = new VulkanIndexBuffer();
	ret->m_device = in_device;
	ret->m_indexCount = (int32_t) indices.size();
	ret->m_indexType = VK_INDEX_TYPE_UINT32;

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VulkanBuffer* stageBuffer = VulkanBuffer::create(
		in_device,
		pool,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		(void *)(indices.data())
	);

	ret->m_buffer = VulkanBuffer::create(
		in_device,
		pool,  
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr
	);

	ret->m_buffer->stageCopyFrom(*stageBuffer, bufferSize,in_device->graphicsQueue);

	delete stageBuffer;
	return ret;
}

VulkanIndexBuffer* VulkanIndexBuffer::create(
	VulkanDevice* vulkanDevice,
	VkDeviceSize bufferSize,
	VkCommandPool pool,
	bool bHeapMemory,
	bool bSSBO)
{
	VulkanIndexBuffer* ret = new VulkanIndexBuffer();

	CHECK(bufferSize > 0);

	ret->m_device = vulkanDevice;
	ret->m_indexCount = (uint32)bufferSize / (uint32)sizeof(uint32);
	ret->m_indexType = VK_INDEX_TYPE_UINT32;

	ret->m_buffer = VulkanBuffer::create(
		vulkanDevice,
		pool,  
		bSSBO ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT :
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		bHeapMemory
	);

	return ret;
}

VkVertexInputBindingDescription VulkanVertexBuffer::getInputBinding(const std::vector<EVertexAttribute>& attributes)
{
	int32_t stride = 0;

	for(int32_t i = 0; i<attributes.size(); i++)
	{
		stride += getVertexAttributeSize(attributes[i]);
	}

	VkVertexInputBindingDescription vertexInputBinding = {};
	vertexInputBinding.binding = 0;
	vertexInputBinding.stride = stride;
	vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return vertexInputBinding;
}

std::vector<VkVertexInputAttributeDescription> VulkanVertexBuffer::getInputAttribute(const std::vector<EVertexAttribute>& shader_inputs)
{
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributs;
	int32_t offset = 0;
	for (int32_t i = 0; i < shader_inputs.size(); i++)
	{
		VkVertexInputAttributeDescription inputAttribute = {};
		inputAttribute.binding  = 0;
		inputAttribute.location = i;
		inputAttribute.format = toVkFormat(shader_inputs[i]);
		inputAttribute.offset = offset;
		offset += getVertexAttributeSize(shader_inputs[i]);
		vertexInputAttributs.push_back(inputAttribute);
	}
	return vertexInputAttributs;
}


VkVertexInputBindingDescription VulkanVertexBuffer::getInputBinding()
{
	return VulkanVertexBuffer::getInputBinding(m_attributes);
}

std::vector<VkVertexInputAttributeDescription> VulkanVertexBuffer::getInputAttribute()
{
	return VulkanVertexBuffer::getInputAttribute(m_attributes);
}

VulkanVertexBuffer* VulkanVertexBuffer::create(
	VulkanDevice* in_device,
	VkCommandPool pool,
	const std::vector<float>& vertices,
	const std::vector<EVertexAttribute>& attributes)
{
	VulkanVertexBuffer* ret = new VulkanVertexBuffer();

	ret->m_device = in_device;
	ret->m_attributes = attributes;
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VulkanBuffer* stageBuffer = VulkanBuffer::create(
		in_device,
		pool,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		(void *)(vertices.data())
	);

	ret->m_buffer = VulkanBuffer::create(
		in_device,
		pool,  
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr
	);

	ret->m_buffer->stageCopyFrom(*stageBuffer, bufferSize,in_device->graphicsQueue);

	delete stageBuffer;
	return ret;
}

VulkanVertexBuffer* VulkanVertexBuffer::create(
	VulkanDevice* in_device,
	VkCommandPool pool,
	VkDeviceSize size,
	bool bHeapMemory,
	bool bSSBO)
{
	VulkanVertexBuffer* ret = new VulkanVertexBuffer();

	ret->m_device = in_device;
	ret->m_buffer = VulkanBuffer::create(
		in_device,
		pool,  
		bSSBO ? VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT :
			    VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		size,
		nullptr,
		bHeapMemory
	);
	return ret;
}

}