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
#include "render_passes/shadowDepth_pass.h"
#include "render_prepare.h"
#include "mesh.h"
#include "compute_passes/gpu_culling.h"
#include "frustum.h"

using namespace engine;

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	1,
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
    return reverseZOpen() ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
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

	m_gpuCullingPass = new GpuCullingPass(this,m_renderScene,shader_compiler, "GpuCulling");

	// 注册RenderPasses
	// m_renderpasses.push_back(new ShadowDepthPass(this,m_renderScene,shader_compiler,"ShadowDepth"));
	m_renderpasses.push_back(new GBufferPass(this,m_renderScene,shader_compiler,"GBuffer"));
	m_renderpasses.push_back(new LightingPass(this,m_renderScene,shader_compiler,"Lighting"));
	m_renderpasses.push_back(new TonemapperPass(this,m_renderScene,shader_compiler,"ToneMapper"));

	// 首先调用PerframeData的初始化函数确保全局的PerframeData正确初始化
	m_frameData.init();
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// 接下来调用每一个Renderpass的初始化函数
	for(auto* pass : m_renderpasses)
	{
		pass->init();
	}

	m_gpuCullingPass->init();

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

	// 首先进行gpu剔除
	m_gpuCullingPass->record(backBufferIndex);

	// 开始动态记录
	VkCommandBuffer dynamicBuf = *VulkanRHI::get()->getDynamicGraphicsCmdBuf(backBufferIndex);

	vkCheck(vkResetCommandBuffer(dynamicBuf,0));
	VkCommandBufferBeginInfo cmdBeginInfo = vkCommandbufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCheck(vkBeginCommandBuffer(dynamicBuf,&cmdBeginInfo));

	MeshLibrary::get()->bindIndexBuffer(dynamicBuf);
	MeshLibrary::get()->bindVertexBuffer(dynamicBuf);

	for(auto* pass : m_renderpasses)
	{
		pass->dynamicRecord(dynamicBuf,backBufferIndex); // 按注册顺序调用
	}

	vkCheck(vkEndCommandBuffer(dynamicBuf));

	uiRecord(backBufferIndex);
	m_uiPass->renderFrame(backBufferIndex);

	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphoreRef();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();
	auto dynamicCmdBufSemaphore = VulkanRHI::get()->getDynamicGraphicsCmdBufSemaphore(backBufferIndex);

	VkSubmitInfo gpuCullingSubmitInfo{};
	gpuCullingSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	gpuCullingSubmitInfo.commandBufferCount = 1;
	gpuCullingSubmitInfo.pCommandBuffers = &m_gpuCullingPass->m_cmdbufs[backBufferIndex]->getInstance();
	gpuCullingSubmitInfo.signalSemaphoreCount = 1;
	gpuCullingSubmitInfo.pSignalSemaphores = &m_gpuCullingPass->m_semaphores[backBufferIndex];
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getComputeQueue(), 1, &gpuCullingSubmitInfo, VK_NULL_HANDLE));

	std::vector<VkPipelineStageFlags> waitFlags = { 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	};

	std::vector<VkSemaphore> waitSemaphores = {
		frameStartSemaphore,	
		m_gpuCullingPass->m_semaphores[backBufferIndex]		
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
	for(auto* pass : m_renderpasses)
	{
		pass->release();
		delete pass;
	}
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
		// NOTE: 当前我们仅处理第一盏有效的直射灯
		for(auto& light:directionalLights)
		{
			if(const auto lightPtr = light.lock())
			{
				bHasValidateDirectionalLight = true;

				m_gpuFrameData.sunLightColor = lightPtr->getColor();
				m_gpuFrameData.sunLightDir = lightPtr->getDirection();
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
	m_gpuFrameData.camWorldPos = glm::vec4(sceneViewCameraComponent->getPosition(),1.0f);

	m_gpuFrameData.cameraInfo = glm::vec4(
		sceneViewCameraComponent->getFoVy(),
		float(m_screenViewportWidth) / float(m_screenViewportHeight),
		sceneViewCameraComponent->getZNear(),
		sceneViewCameraComponent->getZFar()
	);

	Frustum camFrusum{};
	camFrusum.update(m_gpuFrameData.camViewProj);

	m_gpuFrameData.camFrustumPlanes[0] = camFrusum.planes[0];
	m_gpuFrameData.camFrustumPlanes[1] = camFrusum.planes[1];
	m_gpuFrameData.camFrustumPlanes[2] = camFrusum.planes[2];
	m_gpuFrameData.camFrustumPlanes[3] = camFrusum.planes[3];
	m_gpuFrameData.camFrustumPlanes[4] = camFrusum.planes[4];
	m_gpuFrameData.camFrustumPlanes[5] = camFrusum.planes[5];
}


VulkanDescriptorAllocator& Renderer::getDynamicDescriptorAllocator(uint32 i)
{ 
	return *m_dynamicDescriptorAllocator[i]; 
}

VulkanDescriptorFactory Renderer::vkDynamicDescriptorFactoryBegin(uint32 i)
{ 
	return VulkanDescriptorFactory::begin(&VulkanRHI::get()->getDescriptorLayoutCache(), m_dynamicDescriptorAllocator[i]); 
}
