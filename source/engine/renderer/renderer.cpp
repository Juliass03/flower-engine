#include "renderer.h"
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui.h"
#include "render_passes/gbuffer_pass.h"
#include "render_passes/tonemapper_pass.h"
#include "material.h"

namespace engine{

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

	m_renderScene->init();
	m_uiPass->initImgui();

	// 注册RenderPasses
	m_renderpasses.push_back(new GBufferPass(this,m_renderScene,shader_compiler,"GBuffer"));
	m_renderpasses.push_back(new TonemapperPass(this,m_renderScene,shader_compiler,"ToneMapper"));

	// 首先调用PerframeData的初始化函数确保全局的PerframeData正确初始化
	m_frameData.init();
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// 接下来调用每一个Renderpass的初始化函数
	MeshRenderpassLayout renderpassLayouts{};
	for(auto* pass : m_renderpasses)
	{
		pass->init();
		if(pass->isPassType(shaderCompiler::EShaderPass::GBuffer))
		{
			renderpassLayouts.gbuffer = pass->getRenderpass();
		}
	}
	m_materialLibrary->init(shader_compiler,renderpassLayouts);
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
	updateGPUData();
	m_frameData.updateFrameData(m_gpuFrameData);
	m_frameData.updateViewData(m_gpuViewData);

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

void Renderer::uiRecord(size_t i)
{
	m_uiPass->newFrame();

	for(auto& callBackPair : m_uiFunctions)
	{
		callBackPair.second(i);
	}
}

void Renderer::updateGPUData()
{
	float gametime = g_timer.gameTime();
	m_gpuFrameData.appTime = glm::vec4(
		g_timer.globalPassTime(),
		gametime,
		glm::sin(gametime),
		glm::cos(gametime)
	);

	m_gpuViewData.view = glm::mat4(1.0f);
	m_gpuViewData.proj = glm::mat4(1.0f);
	m_gpuViewData.viewProj = glm::mat4(1.0f);
	m_gpuViewData.worldPos = glm::vec4(0.0f);
}



}