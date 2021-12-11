#include "renderer.h"
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui.h"
#include "render_passes/gbuffer_pass.h"
#include "render_passes/tonemapper_pass.h"
#include "material.h"
#include "../scene/components/sceneview_camera.h"
#include "../scene/components/directionalLight.h"
#include "render_passes/lighting_pass.h"
#include "render_passes/cascade_shadowdepth_pass.h"
#include "compute_passes/depth_evaluate_minmax.h"
#include "compute_passes/cascade_setup.h"
#include "render_prepare.h"
#include "mesh.h"
#include "compute_passes/gpu_culling.h"
#include "compute_passes/brdf_lut.h"
#include "compute_passes/irradiance_prefiltercube.h"
#include "compute_passes/hdri2cubemap.h"
#include "compute_passes/specular_prefilter.h"
#include "frustum.h"
#include <renderdoc/renderdoc_app.h>
#include "compute_passes/taa.h"
#include "compute_passes/downsample.h"
#include "render_passes/bloom.h"

#ifdef FXAA_EFFECT
	#include "compute_passes/fxaa.h"
#endif

using namespace engine;

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	1, // Now ReverseZ Always Open, Don't Change Value Please.
	CVarFlags::InitOnce | CVarFlags::ReadOnly
);

static AutoCVarFloat cVarExposure(
	"r.Shading.Exposure",
	"Exposure value.",
	"Shading",
	1.0f,
	CVarFlags::InitOnce | CVarFlags::ReadOnly
);

static AutoCVarInt32 cVarRenderDocCapture(
	"RenderDoc.Capture",
	"Capture Renderdoc.",
	"RenderDoc",
	0,
	CVarFlags::ReadAndWrite
);

static AutoCVarInt32 cVarTAAOpen(
	"r.TAA",
	"Open TAA. 0 is off, 1 is on.",
	"Shading",
	1,
	CVarFlags::ReadAndWrite
);

bool engine::TAAOpen()
{
	return cVarTAAOpen.get() != 0;
}

RENDERDOC_API_1_0_0* getRenderDocApi()
{
	RENDERDOC_API_1_0_0* rdoc = nullptr;
	HMODULE module = GetModuleHandleA("renderdoc.dll");

	if(module==NULL)
	{
		return nullptr;
	}

	pRENDERDOC_GetAPI getApi = nullptr;
	getApi = (pRENDERDOC_GetAPI)GetProcAddress(module,"RENDERDOC_GetAPI");
	if(getApi == nullptr)
	{
		return nullptr;
	}
	if(getApi(eRENDERDOC_API_Version_1_0_0,(void**)&rdoc)!=1)
	{
		return nullptr;
	}
	return rdoc;
}

static RENDERDOC_API_1_0_0* rdc = nullptr;
static bool bRenderDocCaptureing = false;
void renderDocProcess()
{
	if(rdc==nullptr)
	{
		rdc = getRenderDocApi();
	}

	if(rdc && cVarRenderDocCapture.get() != 0)
	{
		if(bRenderDocCaptureing)
		{
			cVarRenderDocCapture.set(0);
			rdc->EndFrameCapture(nullptr, nullptr); 
			bRenderDocCaptureing = false;
		}
		else
		{
			rdc->StartFrameCapture(nullptr, nullptr); 
			bRenderDocCaptureing = true;
		}
	}
}

inline float Halton(uint64_t index, uint64_t base)
{
	float f = 1; float r = 0;
	while (index > 0)
	{
		f = f / static_cast<float>(base);
		r = r + f * (index % base);
		index = index / base;
	}
	return r;
}

inline glm::vec2 Halton2D(uint64_t index, uint64_t baseA, uint64_t baseB)
{
	return glm::vec2(Halton(index, baseA), Halton(index, baseB));
}

float engine::getExposure()
{
	return cVarExposure.get();
}

bool engine::reverseZOpen()
{
    return cVarReverseZ.get() > 0;
}

float engine::getEngineClearZFar()
{
    return engine::reverseZOpen() ? 0.0f : 1.0f;
}

float engine::getEngineClearZNear()
{
    return engine::reverseZOpen() ? 1.0f : 0.0f;
}

VkCompareOp engine::getEngineZTestFunc()
{
    return reverseZOpen() ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
}

Renderer::Renderer(Ref<ModuleManager> in) : IRuntimeModule(in)
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	Ref<SceneManager> sceneManager = m_moduleManager->getRuntimeModule<SceneManager>();
	m_renderScene = new RenderScene(sceneManager,shader_compiler);
	m_uiPass = new ImguiPass();
}

bool Renderer::init()
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	CHECK(shader_compiler);

	m_dynamicDescriptorAllocator.resize(VulkanRHI::get()->getSwapchainImageViews().size());
	for(size_t i = 0; i<m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i] = new VulkanDescriptorAllocator();
		m_dynamicDescriptorAllocator[i]->init(VulkanRHI::get()->getVulkanDevice());
	}

	m_renderScene->init(this);
	m_uiPass->initImgui();

	prepareBasicTextures();
	

	// 注册RenderPasses
	// m_renderpasses.push_back(new ShadowDepthPass(this,m_renderScene,shader_compiler,"ShadowDepth"));

	m_gbufferCullingPass = new GpuCullingPass(this,m_renderScene,shader_compiler, "GbufferCulling");

	m_cascasdeCullingPasses = new GpuCullingPass(this,m_renderScene,shader_compiler, "CascadeCulling");
	m_shadowdepthPasses = new ShadowDepthPass(this,m_renderScene,shader_compiler, "CascadeDepth");

	m_gbufferPass = new GBufferPass(this,m_renderScene,shader_compiler,"GBuffer");
	m_depthEvaluateMinMaxPass = new GpuDepthEvaluateMinMaxPass(this,m_renderScene,shader_compiler, "DepthEvaluateMinMax");
	m_cascadeSetupPass = new GpuCascadeSetupPass(this,m_renderScene,shader_compiler, "CascadeSetup");

	m_lightingPass = new LightingPass(this,m_renderScene,shader_compiler,"Lighting");
	m_downsamplePass = new DownSamplePass(this,m_renderScene,shader_compiler,"Downsample");
	m_bloomPass = new BloomPass(this,m_renderScene,shader_compiler,"Bloom");
	m_taaPass = new TAAPass(this,m_renderScene,shader_compiler,"TAA");
	m_tonemapperPass = new TonemapperPass(this,m_renderScene,shader_compiler,"ToneMapper");

#ifdef FXAA_EFFECT
	m_fxaaPass = new FXAAPass(this,m_renderScene,shader_compiler,"FXAA");
#endif

	// 首先调用PerframeData的初始化函数确保全局的PerframeData正确初始化
	m_frameData.init();
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// 接下来调用每一个Renderpass的初始化函数
	m_gbufferCullingPass->init();
	m_gbufferPass->init();
	m_depthEvaluateMinMaxPass->init();
	m_cascadeSetupPass->init();

	m_cascasdeCullingPasses->init();
	m_shadowdepthPasses->init();

	m_lightingPass->init();
	m_downsamplePass->init();
	m_bloomPass->init();

	m_taaPass->init();
	m_tonemapperPass->init();

#ifdef FXAA_EFFECT
	m_fxaaPass->init();
#endif

	return true;
}

void Renderer::tick(float dt)
{
	renderDocProcess();

	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	CHECK(shader_compiler);

	uint32 backBufferIndex = VulkanRHI::get()->acquireNextPresentImage();
	
	bool bForceAllocate = false;
	if(shaderCompiler::g_shaderPassChange)
	{
		bForceAllocate = true;
		shaderCompiler::g_shaderPassChange = false;
	}

	if( bForceAllocate ||
	   m_screenViewportWidth  != m_renderScene->getSceneTextures().getWidth() ||
	   m_screenViewportHeight != m_renderScene->getSceneTextures().getHeight())
	{
		// NOTE: 发生变化需要重置动态描述符池并重新创建全局的描述符
		m_dynamicDescriptorAllocator[backBufferIndex]->resetPools();
		m_frameData.markPerframeDescriptorSetsDirty();
	}

	// NOTE: 通过initFrame申请到合适的RT后在frameData中构建描述符集
	m_renderScene->initFrame(m_screenViewportWidth, m_screenViewportHeight,bForceAllocate);
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// 更新全局的frameData
	updateGPUData(dt);

	m_frameData.updateFrameData(m_gpuFrameData);
	m_renderScene->renderPrepare(m_gpuFrameData);
	m_renderScene->getSceneTextures().frameBegin();
	
	m_gbufferCullingPass->gbuffer_record(backBufferIndex); // gbuffer culling
	m_gbufferPass->dynamicRecord(backBufferIndex); // gbuffer rendering
	m_depthEvaluateMinMaxPass->record(backBufferIndex); // depth evaluate min max

	m_cascadeSetupPass->record(backBufferIndex);// cascade setup.

	m_cascasdeCullingPasses->cascade_record(backBufferIndex); // cascade culling

	m_shadowdepthPasses->dynamicRecord(backBufferIndex); // shadow rendering

	m_lightingPass->dynamicRecord(backBufferIndex); // lighting pass

	m_downsamplePass->record(backBufferIndex);
	m_bloomPass->dynamicRecord(backBufferIndex);

	m_taaPass->record(backBufferIndex);
	m_tonemapperPass->dynamicRecord(backBufferIndex); // tonemapper pass

#ifdef FXAA_EFFECT
	m_fxaaPass->record(backBufferIndex);
#endif

	uiRecord(backBufferIndex);
	m_uiPass->renderFrame(backBufferIndex);

	// TODO: 使用 rendergraph 简化这部分
	// 提交部分
	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphoreRef();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();

	const bool bSceneEmpty = m_renderScene->isSceneEmpty();

	std::vector<VkPipelineStageFlags> graphicsWaitFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	std::vector<VkPipelineStageFlags> computeWaitFlags = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

	std::vector<VkSemaphore> waitSemaphores = {frameStartSemaphore};



	// 1. gbuffer culling
	VulkanSubmitInfo gbufferCullingSubmitInfo{};
	auto gbufferCullingSemaphore = m_gbufferCullingPass->getSemaphore(backBufferIndex);
	VkCommandBuffer gbufferCullingBuf = m_gbufferCullingPass->getCommandBuf(backBufferIndex)->getInstance();
	gbufferCullingSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(waitSemaphores.data(),(uint32)waitSemaphores.size()) // 等待上一帧完成
		.setSignalSemaphore(&gbufferCullingSemaphore,1)
		.setCommandBuffer(&gbufferCullingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&gbufferCullingSubmitInfo.get(),nullptr));

	// 2. gbuffer rendering
	VulkanSubmitInfo gbufferRenderingSubmitInfo{};
	auto gbufferRenderingSemaphore = m_gbufferPass->getSemaphore(backBufferIndex);
	VkCommandBuffer gbufferRenderingBuf = m_gbufferPass->getCommandBuf(backBufferIndex)->getInstance();
	gbufferRenderingSubmitInfo.setWaitStage(computeWaitFlags)// 等待剔除完成
		.setWaitSemaphore(&gbufferCullingSemaphore,1) 
		.setSignalSemaphore(&gbufferRenderingSemaphore,1) 
		.setCommandBuffer(&gbufferRenderingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&gbufferRenderingSubmitInfo.get(),nullptr));

	// 3. depth evaluate min max
	VulkanSubmitInfo depthEvaluateMinMaxSubmitInfo{};
	auto depthEvaluateMinMaxSemaphore = m_depthEvaluateMinMaxPass->getSemaphore(backBufferIndex);
	VkCommandBuffer depthEvaluateMinMaxBuf = m_depthEvaluateMinMaxPass->getCommandBuf(backBufferIndex)->getInstance();
	depthEvaluateMinMaxSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&gbufferRenderingSemaphore,1) 
		.setSignalSemaphore(&depthEvaluateMinMaxSemaphore,1) 
		.setCommandBuffer(&depthEvaluateMinMaxBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&depthEvaluateMinMaxSubmitInfo.get(),nullptr));

	// 4. cascade setup
	VulkanSubmitInfo cascasdeSetupSubmitInfo{};
	auto cascasdeSetupSemaphore = m_cascadeSetupPass->getSemaphore(backBufferIndex);
	VkCommandBuffer cascasdeSetupBuf = m_cascadeSetupPass->getCommandBuf(backBufferIndex)->getInstance();
	cascasdeSetupSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&depthEvaluateMinMaxSemaphore,1)
		.setSignalSemaphore(&cascasdeSetupSemaphore,1) 
		.setCommandBuffer(&cascasdeSetupBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&cascasdeSetupSubmitInfo.get(),nullptr));

	// 5. cascade culling
	VulkanSubmitInfo cascasdeCullingSubmitInfo{};
	auto cascasdeCullingSemaphore = m_cascasdeCullingPasses->getSemaphore(backBufferIndex);
	VkCommandBuffer cascasdeCullingBuf = m_cascasdeCullingPasses->getCommandBuf(backBufferIndex)->getInstance();
	cascasdeCullingSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&cascasdeSetupSemaphore,1)
		.setSignalSemaphore(&cascasdeCullingSemaphore,1) 
		.setCommandBuffer(&cascasdeCullingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&cascasdeCullingSubmitInfo.get(),nullptr));

	// 6. shadow rendering
	VulkanSubmitInfo shadowDepthSubmitInfo{};
	auto shadowDepthSemaphore = m_shadowdepthPasses->getSemaphore(backBufferIndex);
	VkCommandBuffer shadowDepthBuf = m_shadowdepthPasses->getCommandBuf(backBufferIndex)->getInstance();
	shadowDepthSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&cascasdeCullingSemaphore,1)
		.setSignalSemaphore(&shadowDepthSemaphore,1) 
		.setCommandBuffer(&shadowDepthBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&shadowDepthSubmitInfo.get(),nullptr));

	// 7. lighting
	VulkanSubmitInfo lightingSubmitInfo{};
	auto lightingSemaphore = m_lightingPass->getSemaphore(backBufferIndex);
	VkCommandBuffer lightingBuf = m_lightingPass->getCommandBuf(backBufferIndex)->getInstance();
	lightingSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&shadowDepthSemaphore,1)
		.setSignalSemaphore(&lightingSemaphore,1) 
		.setCommandBuffer(&lightingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&lightingSubmitInfo.get(),nullptr));

	// 8. downsample
	VulkanSubmitInfo downsampleSubmitInfo{};
	auto downsampleSemaphore = m_downsamplePass->getSemaphore(backBufferIndex);
	VkCommandBuffer downsampleBuf = m_downsamplePass->getCommandBuf(backBufferIndex)->getInstance();
	downsampleSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&lightingSemaphore,1)
		.setSignalSemaphore(&downsampleSemaphore,1)
		.setCommandBuffer(&downsampleBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&downsampleSubmitInfo.get(),nullptr));

	// 8.5 bloom
	VulkanSubmitInfo bloomSubmitInfo{};
	auto bloomSemaphore = m_bloomPass->getSemaphore(backBufferIndex);
	VkCommandBuffer bloomBuf = m_bloomPass->getCommandBuf(backBufferIndex)->getInstance();
	bloomSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&downsampleSemaphore,1)
		.setSignalSemaphore(&bloomSemaphore,1)
		.setCommandBuffer(&bloomBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&bloomSubmitInfo.get(),nullptr));

	// 9. taa
	VulkanSubmitInfo taaSubmitInfo{};
	auto taaSemaphore = m_taaPass->getSemaphore(backBufferIndex);
	VkCommandBuffer taaBuf = m_taaPass->getCommandBuf(backBufferIndex)->getInstance();
	taaSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&bloomSemaphore,1)
		.setSignalSemaphore(&taaSemaphore,1) 
		.setCommandBuffer(&taaBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&taaSubmitInfo.get(),nullptr));

	// 10. tonemapping
	VulkanSubmitInfo tonemapperSubmitInfo{};
	auto tonemapperSemaphore = m_tonemapperPass->getSemaphore(backBufferIndex);
	VkCommandBuffer tonemapperBuf = m_tonemapperPass->getCommandBuf(backBufferIndex)->getInstance();
	tonemapperSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&taaSemaphore,1)
		.setSignalSemaphore(&tonemapperSemaphore,1) 
		.setCommandBuffer(&tonemapperBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&tonemapperSubmitInfo.get(),nullptr));

#ifdef FXAA_EFFECT
	// 10.5 fxaa
	VulkanSubmitInfo fxaaSubmitInfo{};
	auto fxaaSemaphore = m_fxaaPass->getSemaphore(backBufferIndex);
	VkCommandBuffer fxaaBuf = m_fxaaPass->getCommandBuf(backBufferIndex)->getInstance();
	fxaaSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&tonemapperSemaphore,1)
		.setSignalSemaphore(&fxaaSemaphore,1) 
		.setCommandBuffer(&fxaaBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&fxaaSubmitInfo.get(),nullptr));
#endif

	// 11. imgui
	VulkanSubmitInfo imguiPassSubmitInfo{};
	VkCommandBuffer cmd_uiPass = m_uiPass->getCommandBuffer(backBufferIndex);
	imguiPassSubmitInfo.setWaitStage(graphicsWaitFlags)
#ifdef FXAA_EFFECT
		.setWaitSemaphore(&fxaaSemaphore,1)
#else
		.setWaitSemaphore(&tonemapperSemaphore,1)
#endif // FXAA_EFFECT
		.setSignalSemaphore(frameEndSemaphore,1)
		.setCommandBuffer(&cmd_uiPass,1);

	std::vector<VkSubmitInfo> submitInfos = { imguiPassSubmitInfo };

	VulkanRHI::get()->resetFence();
	VulkanRHI::get()->submit((uint32_t)submitInfos.size(),submitInfos.data());
	m_uiPass->updateAfterSubmit();


	VulkanRHI::get()->present();
}

void Renderer::release()
{
	m_cascasdeCullingPasses->release(); delete m_cascasdeCullingPasses;
	m_shadowdepthPasses->release(); delete m_shadowdepthPasses;

	m_gbufferPass->release(); delete m_gbufferPass;
	m_depthEvaluateMinMaxPass->release(); delete m_depthEvaluateMinMaxPass;
	m_cascadeSetupPass->release(); delete m_cascadeSetupPass;

	m_lightingPass->release(); delete m_lightingPass;

	m_downsamplePass->release(); delete m_downsamplePass;
	m_bloomPass->release(); delete m_bloomPass;

	m_taaPass->release(); delete m_taaPass;
	m_tonemapperPass->release(); delete m_tonemapperPass;
	m_gbufferCullingPass->release(); delete m_gbufferCullingPass;

#ifdef FXAA_EFFECT
	m_fxaaPass->release(); delete m_fxaaPass;
#endif
	m_uiPass->release(); delete m_uiPass;

	m_renderScene->release(); delete m_renderScene;
	m_frameData.release();

	for(size_t i = 0; i < m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i]->cleanup();
		delete m_dynamicDescriptorAllocator[i];
	}
}

void Renderer::UpdateScreenSize(uint32 width,uint32 height)
{
	const uint32 renderSceneWidth = glm::max(width,ScreenTextureInitSize);
	const uint32 renderSceneHeight = glm::max(height,ScreenTextureInitSize);
	m_screenViewportWidth = renderSceneWidth;
	m_screenViewportHeight = renderSceneHeight;
}

void Renderer::uiRecord(size_t i)
{
	m_uiPass->newFrame();

	for(auto& callBackPair : m_uiFunctions)
	{
		callBackPair.second(i);
	}
}

void Renderer::updateGPUData(float dt)
{
	float gametime = g_timer.gameTime();
	m_gpuFrameData.appTime = glm::vec4(
		g_timer.globalPassTime(),
		gametime,
		glm::sin(gametime),
		glm::cos(gametime)
	);

	auto directionalLights = m_renderScene->getActiveScene().getComponents<DirectionalLight>();
	bool bHasValidateDirectionalLight = false;

	if(directionalLights.size()>0)
	{
		for(auto& light:directionalLights)
		{
			if(const auto lightPtr = light.lock())
			{
				bHasValidateDirectionalLight = true;

				m_gpuFrameData.sunLightColor = lightPtr->getColor();
				m_gpuFrameData.sunLightDir = lightPtr->getDirection();

				break;// NOTE: 当前我们仅处理第一盏有效的直射灯 
			}
		}
	}

	if(!bHasValidateDirectionalLight)
	{
		m_gpuFrameData.sunLightColor = glm::vec4(1.0f,1.0f,1.0f,1.0f);
		m_gpuFrameData.sunLightDir = glm::vec4(0.0f,0.34f,0.33f,1.0f);
	}
	

	auto sceneViewCameraNode = m_renderScene->getActiveScene().getSceneViewCameraNode();
	auto sceneViewCameraComponent = sceneViewCameraNode->getComponent<SceneViewCamera>();

	// 更新场景相机
	sceneViewCameraComponent->tick(dt,m_screenViewportWidth,m_screenViewportHeight);

	m_gpuFrameData.frameIndex.r = m_frameCount;
	m_gpuFrameData.camViewProjLast = m_gpuFrameData.camViewProj;
	m_lastVP = m_gpuFrameData.camViewProjLast;
	m_gpuFrameData.camView = sceneViewCameraComponent->getView();
	m_gpuFrameData.camProj = sceneViewCameraComponent->getProjection();

	if(TAAOpen())
	{
		m_gpuFrameData.jitterData.z = m_gpuFrameData.jitterData.x;
		m_gpuFrameData.jitterData.w = m_gpuFrameData.jitterData.y;

		const float scale               = 1.0f;
		const uint8_t samples           = 16;
		const uint64_t index            = m_frameCount % samples;
		glm::vec2 taaJitter  = Halton2D(index, 2, 3) * 2.0f - 1.0f;

		m_gpuFrameData.jitterData.x   = (taaJitter.x / (float)m_screenViewportWidth)  * scale;
		m_gpuFrameData.jitterData.y   = (taaJitter.y / (float)m_screenViewportHeight) * scale;

#if 0
		// we jitter in vertex shader.
		// it looks like more convenience.
		glm::mat4 jitterMat = glm::mat4(1.0f);
		jitterMat[3][0] = m_gpuFrameData.jitterData.x;
		jitterMat[3][1] = m_gpuFrameData.jitterData.y;
		m_gpuFrameData.camProj = jitterMat * m_gpuFrameData.camProj;

		//m_gpuFrameData.camProj[2][0] += m_gpuFrameData.jitterData.x;
		//m_gpuFrameData.camProj[2][1] += m_gpuFrameData.jitterData.y;
#endif	
	}
	else
	{
		m_gpuFrameData.jitterData = glm::vec4(0.0f);
	}
	
	m_gpuFrameData.camViewProj = m_gpuFrameData.camProj * m_gpuFrameData.camView;
	m_currentVP = m_gpuFrameData.camViewProj;
	m_gpuFrameData.camInvertViewProj = glm::inverse(m_gpuFrameData.camViewProj);
	
	m_gpuFrameData.camWorldPos = glm::vec4(sceneViewCameraComponent->getPosition(),1.0f);

	m_gpuFrameData.cameraInfo = glm::vec4(
		sceneViewCameraComponent->getFoVy(),
		float(m_screenViewportWidth) / float(m_screenViewportHeight),
		reverseZOpen() ? sceneViewCameraComponent->getZFar() : sceneViewCameraComponent->getZNear(),
		reverseZOpen() ? sceneViewCameraComponent->getZNear() : sceneViewCameraComponent->getZFar()
	);

	Frustum camFrusum{};
	camFrusum.update(m_gpuFrameData.camViewProj);

	// 更新ViewFrustum信息
	m_gpuFrameData.camFrustumPlanes[0] = camFrusum.planes[0];
	m_gpuFrameData.camFrustumPlanes[1] = camFrusum.planes[1];
	m_gpuFrameData.camFrustumPlanes[2] = camFrusum.planes[2];
	m_gpuFrameData.camFrustumPlanes[3] = camFrusum.planes[3];
	m_gpuFrameData.camFrustumPlanes[4] = camFrusum.planes[4];
	m_gpuFrameData.camFrustumPlanes[5] = camFrusum.planes[5];

	updateCameraStopFactor();


	// next frame count.
	m_frameCount ++;
}


VulkanDescriptorAllocator& Renderer::getDynamicDescriptorAllocator(uint32 i)
{ 
	return *m_dynamicDescriptorAllocator[i]; 
}

VulkanDescriptorFactory Renderer::vkDynamicDescriptorFactoryBegin(uint32 i)
{ 
	return VulkanDescriptorFactory::begin(&VulkanRHI::get()->getDescriptorLayoutCache(), m_dynamicDescriptorAllocator[i]); 
}

void engine::Renderer::prepareBasicTextures()
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	VulkanSubmitInfo submitInfo{};

	{
		m_renderScene->getSceneTextures().getBRDFLut()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_GENERAL
		);

		GpuBRDFLutPass* brdflutPass = new GpuBRDFLutPass(this,m_renderScene,shader_compiler, "BRDFLutPass");
		brdflutPass->init();
		
		brdflutPass->record(0);
		submitInfo.setCommandBuffer(brdflutPass->getCommandBuf(0),1);
		vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&submitInfo.get(),nullptr));
		vkQueueWaitIdle(VulkanRHI::get()->getGraphicsQueue());
		
		brdflutPass->release();
		delete brdflutPass;

		m_renderScene->getSceneTextures().getBRDFLut()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

#if 0
	{
		m_renderScene->getSceneTextures().getEnvCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_GENERAL
		);

		GpuHDRI2CubemapPass* hdri2CubePass = new GpuHDRI2CubemapPass(this,m_renderScene,shader_compiler, "HDRI2Cubemap");
		hdri2CubePass->init();

		hdri2CubePass->record(0);
		submitInfo.setCommandBuffer(hdri2CubePass->getCommandBuf(0),1);
		vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&submitInfo.get(),nullptr));
		vkQueueWaitIdle(VulkanRHI::get()->getGraphicsQueue());

		hdri2CubePass->release();
		delete hdri2CubePass;

		m_renderScene->getSceneTextures().getEnvCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}
#endif
	
	{
		m_renderScene->getSceneTextures().getIrradiancePrefilterCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_GENERAL
		);

		GpuIrradiancePrefilterPass* irradiancePass = new GpuIrradiancePrefilterPass(this,m_renderScene,shader_compiler, "IrradiancePrefilterPass");
		irradiancePass->init();

		irradiancePass->record(0);
		submitInfo.setCommandBuffer(irradiancePass->getCommandBuf(0),1);
		vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&submitInfo.get(),nullptr));
		vkQueueWaitIdle(VulkanRHI::get()->getGraphicsQueue());

		irradiancePass->release();
		delete irradiancePass;

		m_renderScene->getSceneTextures().getIrradiancePrefilterCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{
		m_renderScene->getSceneTextures().getSpecularPrefilterCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_GENERAL
		);

		GpuSpecularPrefilterPass* specularPass = new GpuSpecularPrefilterPass(this,m_renderScene,shader_compiler, "SpecularPrefilterPass");
		specularPass->init();

		specularPass->record(0);
		submitInfo.setCommandBuffer(specularPass->getCommandBuf(0),1);
		vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&submitInfo.get(),nullptr));
		vkQueueWaitIdle(VulkanRHI::get()->getGraphicsQueue());

		specularPass->release();
		delete specularPass;

		m_renderScene->getSceneTextures().getSpecularPrefilterCube()->transitionLayoutImmediately(
			VulkanRHI::get()->getGraphicsCommandPool(),
			VulkanRHI::get()->getGraphicsQueue(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}
}

uint32 Renderer::getFrameCount() const
{
	return m_frameCount;
}

bool Renderer::cameraMove() const
{
	return m_lastVP != m_currentVP;
}

float Renderer::getCameraStopFactor() const
{
	return cameraStopFactor;
}

void Renderer::updateCameraStopFactor()
{
	if(cameraMove())
	{
		cameraStopFactor = 0.0f;

		bStopMoveView = false;
		return;
	}

	if(!bStopMoveView)
	{
		// first frame stop move view.
		bStopMoveView = true;

		m_cameraStopMoveFrameCount = m_frameCount;
	}

	CHECK(m_frameCount >= m_cameraStopMoveFrameCount);

	float groundCount = 20.0f;
	float differCount = float(std::min(81u ,m_frameCount - m_cameraStopMoveFrameCount));

	cameraStopFactor = glm::clamp(differCount / groundCount, 0.0f, 1.0f);
}
