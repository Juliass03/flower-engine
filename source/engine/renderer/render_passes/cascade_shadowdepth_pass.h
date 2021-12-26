#pragma once
#include "../pass_interface.h"

namespace engine
{

enum class ECullIndex;

// Cpu cascade setup.
// Now we all transfer to gpu.
// See casacde_setup.cpp and cascade_setup.h
struct Cascade
{
	float splitDepth;
	glm::mat4 viewProj;
	glm::vec4 extents;

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

struct PMXCascadePushConstants 
{
	glm::mat4 modelMatrix;
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

	virtual void dynamicRecord(uint32 backBufferIndex) override;
	
private:
	std::vector<std::vector<VkFramebuffer>> m_cascadeFramebuffers = {};
	void cascadeRecord(VkCommandBuffer& cmd,uint32 backBufferIndex, ECullIndex);

	void pmxCascadeRecord(VkCommandBuffer& cmd,uint32 backBufferIndex, ECullIndex);

private:
	bool bInitPipeline = false;
	std::vector<VkPipeline> m_pipelines = {};
	std::vector<VkPipelineLayout> m_pipelineLayouts = {};

	// additional pipeline for pmx mesh.
	std::vector<VkPipeline> m_pmxPipelines = {};
	std::vector<VkPipelineLayout> m_pmxPipelineLayouts = {};

	VkRenderPass m_pmxRenderpass;

	void createRenderpass();
	void destroyRenderpass();

	void createFramebuffers();
	void destroyFramebuffers();

	void createPipeline();
	void destroyPipeline();
};

}