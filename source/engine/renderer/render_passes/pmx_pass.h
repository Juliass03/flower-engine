#pragma once
#include "../pass_interface.h"

namespace engine
{
	struct PMXGpuPushConstants
	{
		glm::mat4 modelMatrix;

		uint32_t basecolorTexId;
	};

	// pmx pass after lighting pass.
	// i prepare a toon style render pipeline for pmx mesh.
	class PMXPass : public GraphicsPass
	{
	public:
		PMXPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
			: GraphicsPass(renderer,scene,sc,name,shaderCompiler::EShaderPass::Unknow)
		{

		}

		virtual void initInner() override;
		virtual void beforeSceneTextureRecreate() override;
		virtual void afterSceneTextureRecreate() override;
		virtual void dynamicRecord(uint32 backBufferIndex) override;

	private:
		std::vector<VkFramebuffer> m_framebuffers = {};

	private:
		bool bInitPipeline = false;
		std::vector<VkPipeline> m_pipelines = {};
		std::vector<VkPipelineLayout> m_pipelineLayouts = {};

		void createRenderpass();
		void destroyRenderpass();

		void createFramebuffers();
		void destroyFramebuffers();

		void createPipeline();
		void destroyPipeline();
	};

}