#pragma once
#include "../pass_interface.h"

namespace engine
{

class ShadowDepthPass : public GraphicsPass
{
public:
	ShadowDepthPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
		:GraphicsPass(renderer,scene,sc,name,shaderCompiler::EShaderPass::Depth)
	{

	}

	virtual void initInner() override;
	virtual void beforeSceneTextureRecreate() override;
	virtual void afterSceneTextureRecreate() override;
	virtual void dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex) override;
private:
	std::vector<VkFramebuffer> m_framebuffers = {};

private:
	void createRenderpass();
	void destroyRenderpass();

	void createFramebuffers();
	void destroyFramebuffers();
};

}