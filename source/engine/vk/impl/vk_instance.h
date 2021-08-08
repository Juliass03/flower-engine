#pragma once
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace engine {

enum class EVulkanVersion
{
	vk_1_0,
	vk_1_1,
	vk_1_2,
};

class VulkanInstance
{
public:
	VulkanInstance() = default;
	~VulkanInstance(){ }

	operator VkInstance()
	{
		return m_instance;
	}

	void init(
		const std::vector<const char *>& required_extensions,
		const std::vector<const char *>& required_validation_layers,
		EVulkanVersion version = EVulkanVersion::vk_1_2,
		const std::string& application_name = "flower");

	void destroy();
	bool isExtensionEnabled(const char *extension) const;
	const std::vector<const char*>& getExtensions(){ return m_enableExtensions; }
	auto getInstance() { return m_instance; }

private:
	VkInstance m_instance = VK_NULL_HANDLE;
	std::vector<const char *> m_enableExtensions = { };
	VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT m_debugReportCallback = VK_NULL_HANDLE;
};

}