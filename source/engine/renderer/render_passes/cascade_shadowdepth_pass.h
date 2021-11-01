#pragma once
#include "../pass_interface.h"

namespace engine
{

enum class ECullIndex;

struct Cascade
{
	float splitDepth;
	glm::mat4 viewProj;

	static void SetupCascades(
		std::vector<Cascade>& inout,
		float nearClip,
		const glm::mat4& cameraViewProj,
		const glm::vec3& lightDir,
		RenderScene* inScene);
};

struct GPUCascadePushConstants 
{
	uint32 cascadeIndex;
};

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

	virtual void dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex) override { }
	void cascadeRecord(VkCommandBuffer& cmd,uint32 backBufferIndex, ECullIndex);
private:
	std::vector<std::vector<VkFramebuffer>> m_cascadeFramebuffers = {};

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