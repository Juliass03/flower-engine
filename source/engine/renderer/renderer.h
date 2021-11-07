#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"
#include "../shader_compiler/shader_compiler.h"
#include "frame_data.h"


namespace engine{

extern bool  reverseZOpen();
extern float getEngineClearZFar();
extern float getEngineClearZNear();
extern VkCompareOp getEngineZTestFunc();

class GraphicsPass;
class GpuCullingPass;
class ShadowDepthPass;
class GpuDepthEvaluateMinMaxPass;
class GpuCascadeSetupPass;

class Renderer : public IRuntimeModule
{
public:
	Renderer(Ref<ModuleManager>);
	
public:
	typedef void (RegisterImguiFunc)(size_t);

	virtual bool init() override;
	virtual void tick(float dt) override;
	virtual void release() override;

	void addImguiFunction(std::string name,std::function<RegisterImguiFunc>&& func)
	{
		this->m_uiFunctions[name] = func; 
	}
	void removeImguiFunction(std::string name)
	{
		this->m_uiFunctions.erase(name);
	}

	void UpdateScreenSize(uint32 width,uint32 height);
	RenderScene& getRenderScene() { return *m_renderScene; }
	PerFrameData& getFrameData() { return m_frameData; }

private:
	ImguiPass* m_uiPass;
	RenderScene* m_renderScene;
	PerFrameData m_frameData { };

	// 动态描述符申请，每次ScreenSize改变时重置。
	std::vector<VulkanDescriptorAllocator*> m_dynamicDescriptorAllocator{};

private:
	void uiRecord(size_t i);

	std::unordered_map<std::string,std::function<RegisterImguiFunc>> m_uiFunctions{};
	bool show_demo_window = true;
	bool show_another_window = false;

	uint32 m_screenViewportWidth = ScreenTextureInitSize;
	uint32 m_screenViewportHeight = ScreenTextureInitSize;

	GPUFrameData m_gpuFrameData { };
	void updateGPUData(float dt);

public:
	VulkanDescriptorAllocator& getDynamicDescriptorAllocator(uint32 i);
	VulkanDescriptorFactory vkDynamicDescriptorFactoryBegin(uint32 i);

public:
	GpuCullingPass* m_gpuCullingPass;

	ShadowDepthPass*   m_shadowdepthPass;
	GraphicsPass*   m_gbufferPass;
	GpuDepthEvaluateMinMaxPass* m_depthEvaluateMinMaxPass;
	GpuCascadeSetupPass* m_cascadeSetupPass;
	GraphicsPass*   m_lightingPass;
	GraphicsPass*   m_tonemapperPass;
};

}