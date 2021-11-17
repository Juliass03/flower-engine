#include "vk_swapchain.h"
#include "vk_device.h"
#include <glfw/glfw3.h>

namespace engine{

void VulkanSwapchain::init(VulkanDevice* in_device,VkSurfaceKHR* in_surface,GLFWwindow* in_window)
{
	m_device = in_device;
	m_surface = in_surface;
	m_window = in_window;

	createSwapchain();
	isOk = true;
}

void VulkanSwapchain::destroy()
{
	if(isOk)
	{
		for (auto imageView : m_swapchainImageViews) 
		{
			vkDestroyImageView(*m_device,imageView,nullptr);
		}
		m_swapchainImageViews.resize(0);

		vkDestroySwapchainKHR(*m_device, m_swapchain, nullptr);
		isOk = false;
	}
}

void VulkanSwapchain::createSwapchain()
{
	auto swapchain_support = m_device->querySwapchainSupportDetail();

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchain_support.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchain_support.presentModes);
	VkExtent2D extent = chooseSwapchainExtent(swapchain_support.capabilities);
	uint32_t imageCount = swapchain_support.capabilities.minImageCount + 1; 								
	if (swapchain_support.capabilities.maxImageCount > 0 && imageCount > swapchain_support.capabilities.maxImageCount) 
	{
		imageCount = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = *m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; 
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = m_device->findQueueFamilies();
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) 
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} 
	else 
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; 
		createInfo.pQueueFamilyIndices = nullptr; 
	}

	createInfo.preTransform = swapchain_support.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE; 

	if (vkCreateSwapchainKHR(*m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) 
	{
		LOG_GRAPHICS_FATAL("Fail to create swapchain.");
	}

	// 申请图片
	vkGetSwapchainImagesKHR(*m_device, m_swapchain, &imageCount, nullptr);
	m_swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(*m_device, m_swapchain, &imageCount, m_swapchainImages.data());

	m_swapchainImageFormat = surfaceFormat.format;
	m_swapchainExtent = extent;

	// 在这里创建SwapChain需要的ImageViews
	m_swapchainImageViews.resize(m_swapchainImages.size());
	for (size_t i = 0; i < m_swapchainImages.size(); i++) 
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; 
		createInfo.format = m_swapchainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(*m_device, &createInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS) 
		{
			LOG_GRAPHICS_FATAL("Fail to create swapchain.");
		}
	}

	LOG_GRAPHICS_TRACE("Create vulkan swapChain succeed, backBuffer count is {0}. ",m_swapchainImageViews.size());
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats) 
	{
		// 尽可能使用srgb_r8g8b8 
		if (
			availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			) 
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX) 
	{
		return capabilities.currentExtent;
	} 
	else 
	{
		int width, height;
		glfwGetFramebufferSize(m_window,&width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}
}