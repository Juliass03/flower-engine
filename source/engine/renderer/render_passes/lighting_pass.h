#pragma once
#include "pass_interface.h"


namespace engine{

class LightingPass : public GraphicsPass
{
public:
	LightingPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
		: GraphicsPass(renderer,scene,sc,name,shaderCompiler::EShaderPass::Lighting)
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

private:
	bool bInitPipeline = false;

	// We need to create global shader here for full screen rendering.
	void createPipeline();
	void destroyPipeline();

	// Lighting pass special vkpipeline.
	std::vector<VkPipeline> m_pipelines = {};
	std::vector<VkPipelineLayout> m_pipelineLayouts = {};

	// Perframe descriptor set.
	std::vector<VulkanDescriptorSetReference> m_lightingPassDescriptorSets = {};
	std::vector<VulkanDescriptorLayoutReference> m_lightingPassDescriptorSetLayouts = {};
};

}