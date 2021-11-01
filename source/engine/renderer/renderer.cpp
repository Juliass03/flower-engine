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
#include "render_prepare.h"
#include "mesh.h"
#include "compute_passes/gpu_culling.h"
#include "frustum.h"

using namespace engine;

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	0,
	CVarFlags::InitOnce | CVarFlags::ReadOnly
);

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

	

	// 注册RenderPasses
	// m_renderpasses.push_back(new ShadowDepthPass(this,m_renderScene,shader_compiler,"ShadowDepth"));

	m_gpuCullingPass = new GpuCullingPass(this,m_renderScene,shader_compiler, "GpuCulling");

	m_shadowdepthPass = new ShadowDepthPass(this,m_renderScene,shader_compiler,"ShadowDepth");
	m_gbufferPass = new GBufferPass(this,m_renderScene,shader_compiler,"GBuffer");
	m_lightingPass = new LightingPass(this,m_renderScene,shader_compiler,"Lighting");
	m_tonemapperPass = new TonemapperPass(this,m_renderScene,shader_compiler,"ToneMapper");

	// 首先调用PerframeData的初始化函数确保全局的PerframeData正确初始化
	m_frameData.init();
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// 接下来调用每一个Renderpass的初始化函数
	m_shadowdepthPass->init();
	m_gpuCullingPass->init();
	m_gbufferPass->init();
	m_lightingPass->init();
	m_tonemapperPass->init();

	return true;
}

void Renderer::tick(float dt)
{
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

	

	VkCommandBuffer dynamicBuf = *VulkanRHI::get()->getDynamicGraphicsCmdBuf(backBufferIndex);

	vkCheck(vkResetCommandBuffer(dynamicBuf,0));
	VkCommandBufferBeginInfo cmdBeginInfo = vkCommandbufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCheck(vkBeginCommandBuffer(dynamicBuf,&cmdBeginInfo));

	MeshLibrary::get()->bindIndexBuffer(dynamicBuf);
	MeshLibrary::get()->bindVertexBuffer(dynamicBuf);

	// TODO: cascade #3 隔3帧更新一次
	//       cascade #2 隔1帧更新一次
	//       cascade #1 和 #0 每帧更新
	m_gpuCullingPass->cascade_record(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_0); // cascade #0 culling
	m_shadowdepthPass->cascadeRecord(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_0);

	m_gpuCullingPass->cascade_record(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_1); // cascade #1 culling
	m_shadowdepthPass->cascadeRecord(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_1);

	m_gpuCullingPass->cascade_record(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_2); // cascade #2 culling
	m_shadowdepthPass->cascadeRecord(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_2);

	m_gpuCullingPass->cascade_record(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_3); // cascade #3 culling
	m_shadowdepthPass->cascadeRecord(dynamicBuf,backBufferIndex, ECullIndex::CASCADE_3);

	m_gpuCullingPass->gbuffer_record(dynamicBuf,backBufferIndex); // gbuffer culling
	m_gbufferPass->dynamicRecord(dynamicBuf,backBufferIndex);

	m_lightingPass->dynamicRecord(dynamicBuf,backBufferIndex);

	m_tonemapperPass->dynamicRecord(dynamicBuf,backBufferIndex);

	vkCheck(vkEndCommandBuffer(dynamicBuf));

	uiRecord(backBufferIndex);
	m_uiPass->renderFrame(backBufferIndex);


// 提交部分
	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphoreRef();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();
	auto dynamicCmdBufSemaphore = VulkanRHI::get()->getDynamicGraphicsCmdBufSemaphore(backBufferIndex);

	const bool bSceneEmpty = m_renderScene->isSceneEmpty();

	std::vector<VkPipelineStageFlags> waitFlags;
	std::vector<VkSemaphore> waitSemaphores;

	waitFlags = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	waitSemaphores = {
		frameStartSemaphore
	};

	VulkanSubmitInfo dynamicBufSubmitInfo{};
	dynamicBufSubmitInfo.setWaitStage(waitFlags)
		.setWaitSemaphore(waitSemaphores.data(),(uint32)waitSemaphores.size())
		.setSignalSemaphore(&dynamicCmdBufSemaphore,1)
		.setCommandBuffer(&dynamicBuf,1);

	VulkanSubmitInfo imguiPassSubmitInfo{};
	VkCommandBuffer cmd_uiPass = m_uiPass->getCommandBuffer(backBufferIndex);
	imguiPassSubmitInfo.setWaitStage(waitFlags)
		.setWaitSemaphore(&dynamicCmdBufSemaphore,1)
		.setSignalSemaphore(frameEndSemaphore,1)
		.setCommandBuffer(&cmd_uiPass,1);

	std::vector<VkSubmitInfo> submitInfos = { dynamicBufSubmitInfo,imguiPassSubmitInfo };

	VulkanRHI::get()->submitAndResetFence((uint32_t)submitInfos.size(),submitInfos.data());
	m_uiPass->updateAfterSubmit();

	VulkanRHI::get()->present();
}

void Renderer::release()
{
	m_shadowdepthPass->release(); delete m_shadowdepthPass;
	m_gbufferPass->release(); delete m_gbufferPass;
	m_lightingPass->release(); delete m_lightingPass;
	m_tonemapperPass->release(); delete m_tonemapperPass;

	m_uiPass->release();
	m_renderScene->release();
	m_frameData.release();
	m_gpuCullingPass->release();


	delete m_gpuCullingPass;
	delete m_uiPass;
	delete m_renderScene;

	for(size_t i = 0; i < m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i]->cleanup();
		delete m_dynamicDescriptorAllocator[i];
	}
}

void Renderer::UpdateScreenSize(uint32 width,uint32 height)
{
	const uint32 renderSceneWidth = glm::max(width,10u);
	const uint32 renderSceneHeight = glm::max(height,10u);
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

	m_gpuFrameData.camView = sceneViewCameraComponent->getView();
	m_gpuFrameData.camProj = sceneViewCameraComponent->getProjection();
	m_gpuFrameData.camViewProj = m_gpuFrameData.camProj * m_gpuFrameData.camView;
	m_gpuFrameData.camInvertViewProj = glm::inverse(m_gpuFrameData.camViewProj);

	m_gpuFrameData.camWorldPos = glm::vec4(sceneViewCameraComponent->getPosition(),1.0f);

	m_gpuFrameData.cameraInfo = glm::vec4(
		sceneViewCameraComponent->getFoVy(),
		float(m_screenViewportWidth) / float(m_screenViewportHeight),
		sceneViewCameraComponent->getZNear(),
		sceneViewCameraComponent->getZFar()
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
	
	// 更新阴影信息
	std::vector<Cascade> cascadeInfos{};
	Cascade::SetupCascades(
		cascadeInfos,
		sceneViewCameraComponent->getZNear(),
		m_gpuFrameData.camViewProj,
		m_gpuFrameData.sunLightDir,
		m_renderScene
	);
	ASSERT(cascadeInfos.size() == 4,"Current use fix 4 cascade num.");
	for(size_t i = 0; i < cascadeInfos.size(); i++)
	{
		m_gpuFrameData.cascadeViewProjMatrix[i] = cascadeInfos[i].viewProj;

		Frustum cascadeFrusum{};
		cascadeFrusum.update(m_gpuFrameData.cascadeViewProjMatrix[i]);

		m_gpuFrameData.cascadeFrustumPlanes[0 + i * 6] =  cascadeFrusum.planes[0];
		m_gpuFrameData.cascadeFrustumPlanes[1 + i * 6] =  cascadeFrusum.planes[1];
		m_gpuFrameData.cascadeFrustumPlanes[2 + i * 6] =  cascadeFrusum.planes[2];
		m_gpuFrameData.cascadeFrustumPlanes[3 + i * 6] =  cascadeFrusum.planes[3];
		m_gpuFrameData.cascadeFrustumPlanes[4 + i * 6] =  cascadeFrusum.planes[4];
		m_gpuFrameData.cascadeFrustumPlanes[5 + i * 6] =  cascadeFrusum.planes[5];
	}

}


VulkanDescriptorAllocator& Renderer::getDynamicDescriptorAllocator(uint32 i)
{ 
	return *m_dynamicDescriptorAllocator[i]; 
}

VulkanDescriptorFactory Renderer::vkDynamicDescriptorFactoryBegin(uint32 i)
{ 
	return VulkanDescriptorFactory::begin(&VulkanRHI::get()->getDescriptorLayoutCache(), m_dynamicDescriptorAllocator[i]); 
}
