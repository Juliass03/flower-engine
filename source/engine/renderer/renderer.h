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
extern float getExposure();
extern bool TAAOpen();

class GraphicsPass;
class GpuCullingPass;
class ShadowDepthPass;
class GpuDepthEvaluateMinMaxPass;
class GpuCascadeSetupPass;
class TAAPass;
class DownSamplePass;
class BloomPass;

extern bool gRenderDocCapture;

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

	uint32 m_frameCount = 0;

	bool bStopMoveView = false;
	uint32 m_cameraStopMoveFrameCount = 0;
	float cameraStopFactor = 0.0f;

	glm::mat4 m_lastVP;
	glm::mat4 m_currentVP;
	void updateCameraStopFactor();

public:
	VulkanDescriptorAllocator& getDynamicDescriptorAllocator(uint32 i);
	VulkanDescriptorFactory vkDynamicDescriptorFactoryBegin(uint32 i);

	uint32 getFrameCount() const;
	float getCameraStopFactor() const;
	bool cameraMove() const;

public:

	GpuCullingPass* m_gbufferCullingPass;
	GraphicsPass*   m_gbufferPass;
	GpuDepthEvaluateMinMaxPass* m_depthEvaluateMinMaxPass;
	GpuCascadeSetupPass* m_cascadeSetupPass;

	GpuCullingPass* m_cascasdeCullingPasses;
	ShadowDepthPass* m_shadowdepthPasses;

	GraphicsPass*   m_lightingPass;
	GraphicsPass* m_pmxPass;
	DownSamplePass* m_downsamplePass;
	BloomPass* m_bloomPass;

	TAAPass* m_taaPass;
	GraphicsPass*   m_tonemapperPass;

private:
	void prepareBasicTextures();
};

}