#pragma once
#include "../pass_interface.h"

namespace engine{

enum class ECullIndex
{
	GBUFFER   = 0,
	CASCADE_0 = 1,
	CASCADE_1 = 2,
	CASCADE_2 = 3,
	CASCADE_3 = 4,
};

inline uint32 cullingIndexToCasacdeIndex(ECullIndex e)
{
	return static_cast<uint32>(e) - 1;
}

struct GPUCullingPushConstants 
{
	uint32 drawCount;

	uint32 cullIndex;
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

	void gbuffer_record(uint32 backBufferIndex);
	void cascade_record(uint32 backBufferIndex);

private:
	bool bInitPipeline = false;

	void createPipeline();
	void destroyPipeline();

	void cascade_record(VkCommandBuffer cmd,uint32 backBufferIndex,ECullIndex cullIndex);

public:
	std::vector<VkPipeline> m_pipelines = {};
	std::vector<VkPipelineLayout> m_pipelineLayouts = {};
};

}