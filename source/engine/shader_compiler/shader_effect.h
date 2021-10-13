#pragma once
#include <string>
#include <vector>
#include "../vk/vk_rhi.h"
#include "../core/file_system.h"
#include "../core/core.h"
#include "../core/runtime_module.h"
#include <bitset>

namespace engine{ namespace shaderCompiler{

class ShaderEffect
{
private:
	struct ShaderStage
	{
		VulkanShaderModule* shaderModule;
		VkShaderStageFlagBits stage;
	};
	std::vector<ShaderStage> m_stages;
	void release();

public:
	~ShaderEffect()
	{
		release();
	}

	// 反射类型覆盖
	struct ReflectionOverrides
	{
		const char* name;
		VkDescriptorType overridenType;
	};

	// 反射绑定
	struct ReflectedBinding
	{
		uint32 set;
		uint32 binding;
		VkDescriptorType type;
	};
	std::unordered_map<std::string/* name */,ReflectedBinding> bindings;

	// 一个DrawCall包含4个描述符集。
	// 1. 帧数据，如时间、鼠标位置等。
	// 2. 视口数据，如相机的ViewProject矩阵、视口大小、视图分辨率等。
	// 3. Pass共有数据，如LightingPass有Gbuffer输入。 
	// 4. 单次DC特有数据，如基础色贴图、模型矩阵等。
	std::array<VkDescriptorSetLayout,4> setLayouts = { VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE };
	std::array<uint32,4> setHashes; // 哈希值以便复用

	VkPipelineLayout builtLayout = VK_NULL_HANDLE; // 缓冲的pipeline layout

	VulkanGraphicsPipelineFactory pipelinebuilder;

	void addStage(VulkanShaderModule* shaderModule,VkShaderStageFlagBits stage);

	void reflectLayout(VkDevice device,ReflectionOverrides* overrides,int overrideCount);

	// 将stages信息填入VkPipelineShaderStageCreateInfo中辅助函数
	void fillStages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages);
};

}}