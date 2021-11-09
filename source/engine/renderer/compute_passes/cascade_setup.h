#pragma once
#include "../pass_interface.h"

namespace engine{

	class GpuCascadeSetupPass: public ComputePass
	{
	public:
		GpuCascadeSetupPass(
			Ref<Renderer> renderer,
			Ref<RenderScene> scene,
			Ref<shaderCompiler::ShaderCompiler> sc,
			const std::string& name)
			: ComputePass(renderer,scene,sc,name)
		{

		}

		virtual void initInner() override;
		virtual void beforeSceneTextureRecreate() override;
		virtual void afterSceneTextureRecreate() override;

		void record(uint32 backBufferIndex);
		void barrierUseStart();
		void barrierUseEnd();

	private:
		bool bInitPipeline = false;

		void createPipeline();
		void destroyPipeline();
	public:
		std::vector<VkPipeline> m_pipelines = {};
		std::vector<VkPipelineLayout> m_pipelineLayouts = {};
	};

}