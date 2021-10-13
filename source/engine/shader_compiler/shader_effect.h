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

	// �������͸���
	struct ReflectionOverrides
	{
		const char* name;
		VkDescriptorType overridenType;
	};

	// �����
	struct ReflectedBinding
	{
		uint32 set;
		uint32 binding;
		VkDescriptorType type;
	};
	std::unordered_map<std::string/* name */,ReflectedBinding> bindings;

	// һ��DrawCall����4������������
	// 1. ֡���ݣ���ʱ�䡢���λ�õȡ�
	// 2. �ӿ����ݣ��������ViewProject�����ӿڴ�С����ͼ�ֱ��ʵȡ�
	// 3. Pass�������ݣ���LightingPass��Gbuffer���롣 
	// 4. ����DC�������ݣ������ɫ��ͼ��ģ�;���ȡ�
	std::array<VkDescriptorSetLayout,4> setLayouts = { VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE };
	std::array<uint32,4> setHashes; // ��ϣֵ�Ա㸴��

	VkPipelineLayout builtLayout = VK_NULL_HANDLE; // �����pipeline layout

	VulkanGraphicsPipelineFactory pipelinebuilder;

	void addStage(VulkanShaderModule* shaderModule,VkShaderStageFlagBits stage);

	void reflectLayout(VkDevice device,ReflectionOverrides* overrides,int overrideCount);

	// ��stages��Ϣ����VkPipelineShaderStageCreateInfo�и�������
	void fillStages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages);
};

}}