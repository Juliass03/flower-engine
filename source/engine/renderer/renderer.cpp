#include "renderer.h"
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui.h"
#include "render_passes/tonemapper_pass.h"

namespace engine{

Renderer::Renderer(Ref<ModuleManager> in): IRuntimeModule(in) {  }

// TODO: 可编程渲染管线
bool Renderer::init()
{
	m_dynamicDescriptorAllocator.resize(VulkanRHI::get()->getSwapchainImageViews().size());
	for(size_t i = 0; i<m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i] = new VulkanDescriptorAllocator();
		m_dynamicDescriptorAllocator[i]->init(VulkanRHI::get()->getVulkanDevice());
	}

	m_renderScene.init();
	m_uiPass.initImgui();

	// 注册RenderPasses
	m_renderpasses.push_back(new TonemapperPass(this,&m_renderScene,"ToneMapper"));

	// 调用初始化函数
	for(auto* pass : m_renderpasses)
	{
		pass->init();
	}

	return true;
}

void Renderer::tick(float dt)
{
	uint32 backBufferIndex = VulkanRHI::get()->acquireNextPresentImage();

	if(m_screenViewportWidth != m_renderScene.getSceneTextures().getWidth() ||
	   m_screenViewportHeight != m_renderScene.getSceneTextures().getHeight())
	{
		// 每帧开始前重置动态描述符池
		m_dynamicDescriptorAllocator[backBufferIndex]->resetPools();
		m_renderScene.initFrame(m_screenViewportWidth, m_screenViewportHeight);
	}

	// 开始动态记录
	VkCommandBuffer dynamicBuf = *VulkanRHI::get()->getDynamicGraphicsCmdBuf(backBufferIndex);

	vkCheck(vkResetCommandBuffer(dynamicBuf,0));
	VkCommandBufferBeginInfo cmdBeginInfo = vkCommandbufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCheck(vkBeginCommandBuffer(dynamicBuf,&cmdBeginInfo));

	// TODO: 改成多线程？
	for(auto* pass : m_renderpasses)
	{
		pass->dynamicRecord(dynamicBuf,backBufferIndex); // 按注册顺序调用
	}

	vkCheck(vkEndCommandBuffer(dynamicBuf));

	uiRecord(backBufferIndex);
	m_uiPass.renderFrame(backBufferIndex);

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
	VkCommandBuffer cmd_uiPass = m_uiPass.getCommandBuffer(backBufferIndex);
	imguiPassSubmitInfo.setWaitStage(waitFlags)
		.setWaitSemaphore(&dynamicCmdBufSemaphore,1)
		.setSignalSemaphore(frameEndSemaphore,1)
		.setCommandBuffer(&cmd_uiPass,1);

	std::vector<VkSubmitInfo> submitInfos = { dynamicBufSubmitInfo,imguiPassSubmitInfo };

	VulkanRHI::get()->submitAndResetFence((uint32_t)submitInfos.size(),submitInfos.data());
	m_uiPass.updateAfterSubmit();

	VulkanRHI::get()->present();
}

void Renderer::release()
{
	for(auto* pass : m_renderpasses)
	{
		pass->release();
		delete pass;
	}
	m_uiPass.release();

	m_renderScene.release();

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
	m_uiPass.newFrame();

	for(auto& callBackPair : m_uiFunctions)
	{
		callBackPair.second(i);
	}
}

}