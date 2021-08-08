#pragma once
#include "vk_common.h"
#include "vk_device.h"
#include <glfw/glfw3.h>

namespace engine{

class VulkanSwapchain
{
private:
	VulkanDevice* m_device  = nullptr;
	VkSurfaceKHR* m_surface = nullptr;
	GLFWwindow*   m_window  = nullptr;
	std::vector<VkImage> m_swapchainImages = {};
	std::vector<VkImageView> m_swapchainImageViews = {};
	VkFormat m_swapchainImageFormat = {};
	VkExtent2D m_swapchainExtent = {};
	VkSwapchainKHR m_swapchain = {};
	bool isOk = false;

private:
	void createSwapchain();
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats); 
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities);

public:
	VulkanSwapchain() = default;
	~VulkanSwapchain(){ destroy(); }
	operator VkSwapchainKHR()
	{
		return m_swapchain;
	}

	void init(VulkanDevice* in_device,VkSurfaceKHR* in_surface,GLFWwindow* in_window);
	void destroy();
	const VkExtent2D& getSwapchainExtent() const { return m_swapchainExtent; }
	auto& getImages() { return m_swapchainImages; }
	VulkanDevice* getDevice() { return m_device; }
	const VkFormat& getSwapchainImageFormat() const { return m_swapchainImageFormat;}
	std::vector<VkImageView>& getImageViews() { return m_swapchainImageViews; }
	VkSwapchainKHR& getInstance(){ return m_swapchain;}

	void rebuildSwapchain()
	{
		CHECK(isOk);
		destroy();
		init(m_device,m_surface,m_window);
	}
};
}