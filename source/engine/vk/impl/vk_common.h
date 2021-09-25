#pragma once
#include <string>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <sstream>
#include "../../core/core.h"

namespace engine{

using VertexIndexType = uint32;

inline constexpr uint32 fnv1a32(char const* s,std::size_t count)
{
	return ((count ? fnv1a32(s,count-1) : 2166136261u) ^ s[count]) * 16777619u;
}

inline uint32 hashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo* info)
{
	std::stringstream ss;

	ss << info->flags;
	ss << info->bindingCount;

	for(auto i = 0u; i<info->bindingCount; i++)
	{
		const VkDescriptorSetLayoutBinding& binding = info->pBindings[i];

		ss << binding.binding;
		ss << binding.descriptorCount;
		ss << binding.descriptorType;
		ss << binding.stageFlags;
	}

	auto str = ss.str();

	return fnv1a32(str.c_str(),str.length());
}

template <class T>
inline std::string vkToString(const T &value)
{
	std::stringstream ss;
	ss << std::fixed << value;
	return ss.str();
}

class VulkanException : public std::runtime_error
{
public:
	VulkanException(VkResult result,const std::string &msg = "vulkan exception"):
		result {result}, std::runtime_error {msg}
	{
		error_message = std::string(std::runtime_error::what())+std::string{" : "} + vkToString(result);
	}

	const char *what() const noexcept override
	{
		return error_message.c_str();
	}

	VkResult result;
private:
	std::string error_message;
};

#ifdef FLOWER_DEBUG
inline void vkCheck(VkResult err)
{
	if (err)
	{ 
		LOG_GRAPHICS_FATAL("check error: {}.", vkToString(err));
		__debugbreak(); 
		throw VulkanException(err);
		abort(); 
	} 
}

inline void vkHandle(decltype(VK_NULL_HANDLE) handle)
{
	if (handle == VK_NULL_HANDLE) 
	{ 
		LOG_GRAPHICS_FATAL("Handle is empty.");
		__debugbreak(); 
		abort(); 
	}
}
#else
inline void vkCheck(VkResult err)
{
}

inline void vkHandle(decltype(VK_NULL_HANDLE) handle)
{
}
#endif // FLOWER_DEBUG

inline uint32 getMipLevelsCount(uint32 texWidth,uint32 texHeight)
{
	return static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
}

inline void executeImmediately(
	VkDevice device,
	VkCommandPool commandPool,
	VkQueue queue,
	const std::function<void(VkCommandBuffer cb)>& func)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	func(commandBuffer);

	vkEndCommandBuffer(commandBuffer);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

enum class EVertexAttribute : uint32
{
	none = 0,
	pos,	        // vec3
	uv0,            // vec2
	uv1,	        // vec2
	normal,         // vec3
	tangent,        // vec4
	color,          // vec3
	alpha,          // float
	count,
};

inline int32 getVertexAttributeSize(EVertexAttribute va)
{
	switch(va)
	{
	case EVertexAttribute::alpha:
		return sizeof(float);
		break;

	case EVertexAttribute::uv0:
	case EVertexAttribute::uv1:
		return 2 * sizeof(float);
		break;

	case EVertexAttribute::pos:
	case EVertexAttribute::color:
	case EVertexAttribute::normal:
		return 3 * sizeof(float);
		break;

	case EVertexAttribute::tangent:
		return 4 * sizeof(float);
		break;

	case EVertexAttribute::none:
	case EVertexAttribute::count:
	default:
		LOG_GRAPHICS_ERROR("Unknown vertex_attribute type.");
		return 0;
		break;
	}
	return 0;
}

inline int32 getVertexAttributeElementCount(EVertexAttribute va)
{
	return getVertexAttributeSize(va) / sizeof(float);
}

inline VkFormat toVkFormat(EVertexAttribute va)
{
	VkFormat format = VK_FORMAT_R32G32B32_SFLOAT;
	switch(va)
	{
	case EVertexAttribute::alpha:
		format = VK_FORMAT_R32_SFLOAT;
		break;

	case EVertexAttribute::uv0:
	case EVertexAttribute::uv1:
		format = VK_FORMAT_R32G32_SFLOAT;
		break;

	case EVertexAttribute::pos:
	case EVertexAttribute::normal:
	case EVertexAttribute::color:
		format = VK_FORMAT_R32G32B32_SFLOAT;
		break;

	case EVertexAttribute::tangent:
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		break;

	case EVertexAttribute::none:
	case EVertexAttribute::count:
	default:
		LOG_GRAPHICS_FATAL("Unkown vertex_attribute type.");
		break;
	}
	return format;
}

}