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

namespace engine{

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	1,
	CVarFlags::InitOnce | CVarFlags::ReadOnly
);

bool reverseZOpen()
{
    return cVarReverseZ.get() > 0;
}

float getEngineClearZFar()
{
    return reverseZOpen() ? 0.0f : 1.0f;
}

float getEngineClearZNear()
{
    return reverseZOpen() ? 1.0f : 0.0f;
}

VkCompareOp getEngineZTestFunc()
{
    return reverseZOpen() ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
}


Renderer::Renderer(Ref<ModuleManager> in):
	IRuntimeModule(in)
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	Ref<SceneManager> sceneManager = m_moduleManager->getRuntimeModule<SceneManager>();
	m_renderScene = new RenderScene(sceneManager,shader_compiler);
	m_uiPass = new ImguiPass();

	m_materialLibrary = new MaterialLibrary();
}

// TODO: 可编程渲染管线
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
	updateRenderpassLayout();
	m_materialLibrary->init(shader_compiler);
	return true;
}

void Renderer::tick(float dt)
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	CHECK(shader_compiler);

	uint32 backBufferIndex = VulkanRHI::get()->acquireNextPresentImage();
	
	bool bForceAllocate = false;
	if(shaderCompiler::g_globalPassShouldRebuild)
	{
		bForceAllocate = true;
		shaderCompiler::g_globalPassShouldRebuild = false;
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
	m_frameData.updateViewData(m_gpuViewData);

	// NOTE: 由于Sceneza

	// 开始动态记录
	VkCommandBuffer dynamicBuf = *VulkanRHI::get()->getDynamicGraphicsCmdBuf(backBufferIndex);

	vkCheck(vkResetCommandBuffer(dynamicBuf,0));
	VkCommandBufferBeginInfo cmdBeginInfo = vkCommandbufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCheck(vkBeginCommandBuffer(dynamicBuf,&cmdBeginInfo));

	for(auto* pass : m_renderpasses)
	{
		pass->dynamicRecord(dynamicBuf,backBufferIndex); // 按注册顺序调用
	}

	vkCheck(vkEndCommandBuffer(dynamicBuf));

	uiRecord(backBufferIndex);
	m_uiPass->renderFrame(backBufferIndex);

	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphore();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();
	auto dynamicCmdBufSemaphore = VulkanRHI::get()->getDynamicGraphicsCmdBufSemaphore(backBufferIndex);

	std::vector<VkPipelineStageFlags> waitFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VulkanSubmitInfo dynamicBufSubmitInfo{};
	dynamicBufSubmitInfo.setWaitStage(waitFlags)
		.setWaitSemaphore(frameStartSemaphore,1)
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
	m_materialLibrary->release();
	delete m_uiPass;
	delete m_renderScene;
	delete m_materialLibrary;

	for(size_t i = 0; i<m_dynamicDescriptorAllocator.size(); i++)
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

void Renderer::updateRenderpassLayout()
{
	for(auto* pass : m_renderpasses)
	{
		if(pass->isPassType(shaderCompiler::EShaderPass::GBuffer))
		{
			m_renderpassLayout.gBufferPass = pass->getRenderpass();
		}
		else if(pass->isPassType(shaderCompiler::EShaderPass::ShadowDepth))
		{
			m_renderpassLayout.shadowDepth = pass->getRenderpass();
		}
	}
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
		m_gpuFrameData.sunLightDir = glm::vec4(0.33f,0.34f,0.33f,1.0f);
	}
	

	auto sceneViewCameraNode = m_renderScene->getActiveScene().getSceneViewCameraNode();
	auto sceneViewCameraComponent = sceneViewCameraNode->getComponent<SceneViewCamera>();

	// 更新场景相机
	sceneViewCameraComponent->tick(dt,m_screenViewportWidth,m_screenViewportHeight);

	m_gpuViewData.view = sceneViewCameraComponent->getView();
	m_gpuViewData.proj = sceneViewCameraComponent->getProjection();
	m_gpuViewData.viewProj = m_gpuViewData.proj * m_gpuViewData.view;
	m_gpuViewData.worldPos = glm::vec4(sceneViewCameraComponent->getPosition(),1.0f);
}

MeshPassLayout Renderer::getMeshpassLayout() const
{
	return m_renderpassLayout;
}



}