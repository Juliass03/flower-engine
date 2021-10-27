#pragma once
#include "../pass_interface.h"

namespace engine{

struct GPUCullingPushConstants 
{
	uint32 drawCount;
};

class GpuCullingPass : public ComputePass
{
public:
	GpuCullingPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
		: ComputePass(renderer,scene,sc,name)
	{
		
	}

	virtual void initInner() override;
	virtual void beforeSceneTextureRecreate() override;
	virtual void afterSceneTextureRecreate() override;

	void record(uint32 index);

private:
	bool bInitPipeline = false;

	void createPipeline();
	void destroyPipeline();

	void createAsyncObjects();
	void destroyAsyncObjects();

public:
	std::vector<VkPipeline> m_pipelines = {};
	std::vector<VkPipelineLayout> m_pipelineLayouts = {};

	std::vector<VulkanCommandBuffer*> m_cmdbufs = {};
	std::vector<VkSemaphore> m_semaphores = {};
};

}