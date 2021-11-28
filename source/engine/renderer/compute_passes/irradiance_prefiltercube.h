#pragma once
#include "../pass_interface.h"

namespace engine{

	class GpuIrradiancePrefilterPass: public ComputePass
	{
	public:
		GpuIrradiancePrefilterPass(
			Ref<Renderer> renderer,
			Ref<RenderScene> scene,
			Ref<shaderCompiler::ShaderCompiler> sc,
			const std::string& name)
			: ComputePass(renderer,scene,sc,name)
		{

		}

		virtual void initInner() override;
		virtual void beforeSceneTextureRecreate() override {}
		virtual void afterSceneTextureRecreate() override {}

		void record(uint32 backBufferIndex);

	private:
		bool bInitPipeline = false;

		void createPipeline();
		void destroyPipeline();

	public:
		std::vector<VkPipeline> m_pipelines = {};
		std::vector<VkPipelineLayout> m_pipelineLayouts = {};

		std::vector<VulkanDescriptorSetReference> m_descriptorSets = {};
		std::vector<VulkanDescriptorLayoutReference> m_descriptorSetLayouts = {};
	};
}