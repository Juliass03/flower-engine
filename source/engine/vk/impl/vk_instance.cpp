#include "vk_instance.h"
#include "vk_common.h"
#include <glfw/glfw3.h>

namespace engine{

static AutoCVarInt32 cVarVulkanOpenValidation(
	"r.RHI.OpenValidation",
	"Enable vulkan validation layer.0 is off,1 is on.",
	"RHI",
	1,
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

inline bool openVulkanValiadation()
{
	return cVarVulkanOpenValidation.get() != 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
	void* user_data)
{
	if(message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LOG_GRAPHICS_WARN(
			"{} - {}: {}",
			callback_data->messageIdNumber,
			callback_data->pMessageIdName,
			callback_data->pMessage
		);
	}
	else if(message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOG_GRAPHICS_ERROR(
			"{} - {}: {}",
			callback_data->messageIdNumber,
			callback_data->pMessageIdName,
			callback_data->pMessage
		);
	}
	return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT /*type*/,
	uint64_t /*object*/,
	size_t /*location*/,
	int32_t /*message_code*/,
	const char *layer_prefix,
	const char *message,
	void * /*user_data*/)
{
	if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		LOG_GRAPHICS_ERROR(
			"{}: {}",
			layer_prefix,
			message
		);
	}
	else if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		LOG_GRAPHICS_WARN(
			"{}: {}",
			layer_prefix,
			message
		);
	}
	else if(flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		LOG_GRAPHICS_WARN(
			"{}: {}",
			layer_prefix,
			message
		);
	}
	else
	{
		LOG_GRAPHICS_INFO(
			"{}: {}",
			layer_prefix,
			message
		);
	}
	return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
	if(func!=nullptr)
	{
		return func(instance,pCreateInfo,pAllocator,pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
	if(func!=nullptr)
	{
		func(instance,debugMessenger,pAllocator);
	}
}

VkResult createDebugReportCallbackEXT(
	VkInstance instance,
	const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugReportCallbackEXT* pDebugReporter)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT");
	if(func!=nullptr)
	{
		return func(instance,pCreateInfo,pAllocator,pDebugReporter);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroyDebugReportCallbackEXT(
	VkInstance instance,
	VkDebugReportCallbackEXT DebugReporter,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT");
	if(func!=nullptr)
	{
		func(instance,DebugReporter,pAllocator);
	}
}

bool validateRequestLayersAvailable(
	const std::vector<const char *>& required,
	const std::vector<VkLayerProperties> &available)
{
	for(auto layer:required)
	{
		bool found = false;
		for(auto &available_layer:available)
		{
			if(strcmp(available_layer.layerName,layer)==0)
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			LOG_GRAPHICS_ERROR ("No validate layer find: {}.",layer);
			return false;
		}
	}
	return true;
}

std::vector<const char*> getOptimalValidationLayers(
	const std::vector<VkLayerProperties> &supported_instance_layers)
{
	std::vector<std::vector<const char *>> validation_layer_priority_list =
	{
		{"VK_LAYER_KHRONOS_validation"},
		{"VK_LAYER_LUNARG_standard_validation"},
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_GOOGLE_unique_objects",
	},
		{"VK_LAYER_LUNARG_core_validation"}
	};

	for(auto &validation_layers:validation_layer_priority_list)
	{
		if(validateRequestLayersAvailable(validation_layers,supported_instance_layers))
		{
			return validation_layers;
		}

		LOG_GRAPHICS_WARN("Can't open validate lyaer! vulkan will run without debug layer.");
	}

	return {};
}


void VulkanInstance::init(
	const std::vector<const char*>& required_extensions,
	const std::vector<const char*>& required_validation_layers,
	EVulkanVersion version,
	const std::string & application_name)
{
	// 1. 查询支持的实例插件
	uint32_t instance_extension_count;
	vkCheck(vkEnumerateInstanceExtensionProperties(nullptr,&instance_extension_count,nullptr));
	std::vector<VkExtensionProperties> available_instance_extensions(instance_extension_count);
	vkCheck(vkEnumerateInstanceExtensionProperties(nullptr,&instance_extension_count,available_instance_extensions.data()));

	// 2. Debug插件开启
	bool debug_utils = false;
	if(openVulkanValiadation())
	{
		for(auto &available_extension:available_instance_extensions)
		{
			if(strcmp(available_extension.extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME)==0)
			{
				debug_utils = true;
				LOG_GRAPHICS_INFO("Plugin {} is useful. opening...",VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				m_enableExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
		}
		if(!debug_utils)
		{
			m_enableExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
	}

	// 增加表面显示插件
	m_enableExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

	// 添加glfw插件
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	for(uint32_t i = 0; i<glfwExtensionCount; i++)
	{
		m_enableExtensions.push_back(extensions[i]);
	}

	// 增加其他可用插件
	for(auto &available_extension:available_instance_extensions)
	{
		if(strcmp(available_extension.extensionName,VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)==0)
		{
			LOG_GRAPHICS_INFO("Plugin {} is useful. opening...",VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
			m_enableExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}
	}

	// 3. 检查要求的插件是否可用
	auto extension_error = false;
	for(auto extension : required_extensions)
	{
		auto extension_name = extension;
		if(std::find_if(available_instance_extensions.begin(),available_instance_extensions.end(),
			[&extension_name](VkExtensionProperties available_extension)
		{
			return strcmp(available_extension.extensionName,extension_name)==0;
		})==available_instance_extensions.end())
		{
			LOG_GRAPHICS_FATAL("Plugin {} is no useful. closing...",extension_name);
			extension_error = true;
		}
		else
		{
			m_enableExtensions.push_back(extension_name);
		}
	}

	if(extension_error)
	{
		LOG_GRAPHICS_FATAL("lose requeset vulkan extension.");
	}

	uint32_t instance_layer_count;
	vkCheck(vkEnumerateInstanceLayerProperties(&instance_layer_count,nullptr));

	std::vector<VkLayerProperties> supported_validation_layers(instance_layer_count);
	vkCheck(vkEnumerateInstanceLayerProperties(&instance_layer_count,supported_validation_layers.data()));

	std::vector<const char *> requested_validation_layers(required_validation_layers);

	if(openVulkanValiadation())
	{
		std::vector<const char *> optimal_validation_layers = getOptimalValidationLayers(supported_validation_layers);
		requested_validation_layers.insert(requested_validation_layers.end(),optimal_validation_layers.begin(),optimal_validation_layers.end());
	}

	if(validateRequestLayersAvailable(requested_validation_layers,supported_validation_layers))
	{
		LOG_GRAPHICS_INFO("Validation layers opened:");
		for(const auto &layer:requested_validation_layers)
		{
			LOG_GRAPHICS_INFO("	\t{}",layer);
		}
	}
	else
	{
		LOG_GRAPHICS_FATAL("No validate layer find.");
	}

	VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};

	app_info.pApplicationName = application_name.c_str();
	app_info.applicationVersion = 0;
	app_info.pEngineName = "flower_engine";
	app_info.engineVersion = 0;

	switch(version)
	{
	case EVulkanVersion::vk_1_0:
		app_info.apiVersion = VK_MAKE_VERSION(1,0,0);
		break;
	case EVulkanVersion::vk_1_1:
		app_info.apiVersion = VK_MAKE_VERSION(1,1,0);
		break;
	case EVulkanVersion::vk_1_2:
		app_info.apiVersion = VK_MAKE_VERSION(1,2,0);
		break;
	default:
		app_info.apiVersion = VK_MAKE_VERSION(1,0,0);
		break;
	}

	VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};

	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledExtensionCount = static_cast<uint32_t>(m_enableExtensions.size());
	instance_info.ppEnabledExtensionNames = m_enableExtensions.data();
	instance_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
	instance_info.ppEnabledLayerNames = requested_validation_layers.data();

	VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info  = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	VkDebugReportCallbackCreateInfoEXT debug_report_create_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
	if(openVulkanValiadation())
	{
		if(debug_utils)
		{
			debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
			debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			debug_utils_create_info.pfnUserCallback = debugUtilsMessengerCallback;
			instance_info.pNext = &debug_utils_create_info;
		}
		else
		{
			debug_report_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT|VK_DEBUG_REPORT_WARNING_BIT_EXT;
			debug_report_create_info.pfnCallback = debugReportCallback;
			instance_info.pNext = &debug_report_create_info;
		}
	}

	// 创建vulkan实例
	auto result = vkCreateInstance(&instance_info,nullptr,&m_instance);
	if(result!=VK_SUCCESS)
	{
		LOG_GRAPHICS_FATAL("Fail to create vulkan instance.");
		throw VulkanException(result,"Fail to create vulkan instance.");
	}

	if(openVulkanValiadation())
	{
		if(debug_utils)
		{
			result = CreateDebugUtilsMessengerEXT(m_instance,&debug_utils_create_info,nullptr,&m_debugUtilsMessenger);
			if(result!=VK_SUCCESS)
			{
				LOG_GRAPHICS_FATAL("Fail to create debug utils messenger.");
				throw VulkanException(result,"Fail to create debug utils messenger.");
			}
		}
		else
		{
			result = createDebugReportCallbackEXT(m_instance,&debug_report_create_info,nullptr,&m_debugReportCallback);
			if(result!=VK_SUCCESS)
			{
				LOG_GRAPHICS_FATAL("Fail to create debug report callback.");
				throw VulkanException(result,"Fail to create debug report callback.");
			}
		}
	}
}

void VulkanInstance::destroy()
{
	if(m_debugUtilsMessenger!=VK_NULL_HANDLE)
	{
		destroyDebugUtilsMessengerEXT(m_instance,m_debugUtilsMessenger,nullptr);
	}
	if(m_debugReportCallback!=VK_NULL_HANDLE)
	{
		destroyDebugReportCallbackEXT(m_instance,m_debugReportCallback,nullptr);
	}

	if(m_instance!=VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance,nullptr);
	}
}

bool VulkanInstance::isExtensionEnabled(const char *extension) const
{ 
	return std::find_if(
		m_enableExtensions.begin(),
		m_enableExtensions.end(),
		[extension](const char *enabled_extension)
	{
		return strcmp(extension,enabled_extension)==0;
	}
	) != m_enableExtensions.end(); 
}

}