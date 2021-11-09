#pragma once
#include "../pass_interface.h"

namespace engine
{

class GBufferPass : public GraphicsPass
{
public:
	GBufferPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
		:GraphicsPass(renderer,scene,sc,name,shaderCompiler::EShaderPass::GBuffer)
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