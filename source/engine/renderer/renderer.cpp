#include "renderer.h"
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui.h"

namespace engine{

Renderer::Renderer(Ref<ModuleManager> in): IRuntimeModule(in) {  }

bool Renderer::init()
{
	VulkanRHI::get()->init(g_windowData.window);
	m_uiPass.initImgui();
	return true;
}

void Renderer::tick(float dt)
{
	uint32 backBufferIndex = VulkanRHI::get()->acquireNextPresentImage();
	uiRecord();
	m_uiPass.renderFrame(backBufferIndex);

	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphore();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();
	std::vector<VkPipelineStageFlags> waitFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VulkanSubmitInfo imguiPassSubmitInfo{};
	VkCommandBuffer cmd_uiPass = m_uiPass.getCommandBuffer(backBufferIndex);
	imguiPassSubmitInfo.setWaitStage(waitFlags)
		.setWaitSemaphore(frameStartSemaphore,1)
		.setSignalSemaphore(frameEndSemaphore,1)
		.setCommandBuffer(&cmd_uiPass,1);

	std::vector<VkSubmitInfo> submitInfos = { imguiPassSubmitInfo };

	VulkanRHI::get()->submitAndResetFence((uint32_t)submitInfos.size(),submitInfos.data());
	m_uiPass.updateAfterSubmit();

	VulkanRHI::get()->present();
}

void Renderer::release()
{
	m_uiPass.releaseImgui();
	VulkanRHI::get()->release();
}

void Renderer::uiRecord()
{
	ImguiPass::newFrame();

	for(auto& callBackPair : m_uiFunctions)
	{
		callBackPair.second();
	}
}

}