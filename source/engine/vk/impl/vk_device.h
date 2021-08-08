#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "vk_common.h"

namespace engine{

class VulkanQueueFamilyIndices 
{
	friend class VulkanDevice;
public:
	uint32_t graphicsFamily; // ͼ�ζ�����
	uint32_t presentFamily;  // ��ʾ������
	uint32_t computeFaimly;  // ���������

	bool isCompleted() 
	{
		return bGraphicsFamilySet && bPresentFamilySet && bComputeFaimlySet;
	}

private:
	bool bGraphicsFamilySet = false;
	bool bPresentFamilySet = false;
	bool bComputeFaimlySet = false;
};

struct VulkanSwapchainSupportDetails 
{
	VkSurfaceCapabilitiesKHR capabilities; 
	std::vector<VkSurfaceFormatKHR> formats; 
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDevice
{
public:
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties physicalDeviceProperties = {};
	VkQueue graphicsQueue = VK_NULL_HANDLE; // ͼ�ζ���
	VkQueue presentQueue = VK_NULL_HANDLE;  // ��ʾ����
	VkQueue computeQueue = VK_NULL_HANDLE;  // �������

private:
	VkInstance m_instance = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	std::vector<const char*> m_deviceExtensions = {};
	VkPhysicalDeviceFeatures m_openFeatures = {};

public:
	operator VkDevice() { return device; }
	VulkanDevice() = default;
	~VulkanDevice(){ }

	void init(
		VkInstance instance,
		VkSurfaceKHR surface,
		VkPhysicalDeviceFeatures features = {},
		const std::vector<const char*>& device_request_extens= {}
	);

	void destroy();
	VulkanQueueFamilyIndices findQueueFamilies();
	uint32 findMemoryType(uint32 typeFilter,VkMemoryPropertyFlags properties);
	VulkanSwapchainSupportDetails querySwapchainSupportDetail();
	void printAllQueueFamiliesInfo();
	const auto& getDeviceExtensions() const { return m_deviceExtensions; }
	const auto& getDeviceFeatures()   const { return m_openFeatures; }

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,VkImageTiling tiling,VkFormatFeatureFlags features);
	VkFormat findDepthStencilFormat();
	VkFormat findDepthOnlyFormat();

private:
	void pickupSuitableGpu(VkInstance& instance,const std::vector<const char*>& device_request_extens);
	bool isPhysicalDeviceSuitable(const std::vector<const char*>& device_request_extens);
	bool checkDeviceExtensionSupport(const std::vector<const char*>& device_request_extens);
	void createLogicDevice();
};
}