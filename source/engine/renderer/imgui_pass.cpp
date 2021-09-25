#include "imgui_pass.h"
#include "../../imgui/imgui.h"
#include <vulkan/vulkan.h>
#include "../../imgui/imgui_impl_glfw.h"
#include "../core/windowData.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui_impl_vulkan.h"
#include "../core/file_system.h"

namespace engine{

static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[imgui - vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

void setupVulkanInitInfo(ImGui_ImplVulkan_InitInfo* inout,VkDescriptorPool pool)
{
	inout->Instance = VulkanRHI::get()->getInstance();
	inout->PhysicalDevice = VulkanRHI::get()->getVulkanDevice()->physicalDevice;
	inout->Device = VulkanRHI::get()->getDevice();
	inout->QueueFamily = VulkanRHI::get()->getVulkanDevice()->findQueueFamilies().graphicsFamily;
	inout->Queue = VulkanRHI::get()->getGraphicsQueue();
	inout->PipelineCache = VK_NULL_HANDLE;
	inout->DescriptorPool = pool;
	inout->Allocator = nullptr;
	inout->MinImageCount = (uint32_t)VulkanRHI::get()->getSwapchainImageViews().size();
	inout->ImageCount = inout->MinImageCount;
	inout->MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	inout->CheckVkResultFn = check_vk_result;
	inout->Subpass = 0;
}

void setupStyle()
{
	ImGui::StyleColorsDark();
	ImGui::GetStyle().FrameRounding = 4;
	ImGui::GetStyle().WindowRounding = 2;
	ImGui::GetStyle().WindowPadding = ImVec2(8.0f,8.0f);
	ImGui::GetStyle().ChildRounding = 4;
	ImGui::GetStyle().FrameRounding = 4;
	ImGui::GetStyle().PopupRounding = 2;
	ImGui::GetStyle().ScrollbarRounding = 4;
	ImGui::GetStyle().GrabRounding = 4;
	ImGui::GetStyle().LogSliderDeadzone = 4;
	ImGui::GetStyle().TabRounding = 4;
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_Right;
	ImGui::GetStyle().WindowBorderSize = 1.0f;
	ImGui::GetStyle().PopupBorderSize = 1.0f;
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.02f, 0.02f, 0.02f, 0.94f);
	colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.13f, 0.13f, 0.14f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.03f, 0.04f, 0.04f, 0.67f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.99f, 0.99f, 0.99f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 1.00f, 1.00f, 0.14f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.43f, 0.44f, 0.46f, 0.40f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.12f, 0.20f, 0.19f, 1.00f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.08f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.29f, 0.29f, 0.29f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator]              = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(0.05f, 0.05f, 0.05f, 0.80f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_TabUnfocused]           = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
	

}

void ImguiPass::renderpassBuild()
{
	// Create the Render Pass
	{
		VkAttachmentDescription attachment = {};
		attachment.format = VulkanRHI::get()->getSwapchainFormat();
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 每帧加载前清除
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment = {};
		color_attachment.attachment = 0;
		color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		VkRenderPassCreateInfo info = {};

		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dependency;
		vkCheck(vkCreateRenderPass(VulkanRHI::get()->getDevice(), &info, nullptr, &this->m_gpuResource.renderPass));
	}

	// Framebuffer直接在Swapchain BackBuffer上画
	// Create Framebuffer & CommandBuffer
	{
		VkImageView attachment[1];
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = this->m_gpuResource.renderPass;
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.width = VulkanRHI::get()->getSwapchainExtent().width;
		info.height = VulkanRHI::get()->getSwapchainExtent().height;
		info.layers = 1;

		auto backBufferSize = VulkanRHI::get()->getSwapchainImageViews().size();
		m_gpuResource.framebuffers.resize(backBufferSize);
		m_gpuResource.commandPools.resize(backBufferSize);
		m_gpuResource.commandBuffers.resize(backBufferSize);

		for (uint32_t i = 0; i < backBufferSize; i++)
		{
			attachment[0] = VulkanRHI::get()->getSwapchainImageViews()[i];
			vkCheck(vkCreateFramebuffer(VulkanRHI::get()->getDevice(), &info, nullptr, &m_gpuResource.framebuffers[i]));
		}
		
		for (uint32_t i = 0; i < backBufferSize; i++)
		{
			// Command pool
			{
				VkCommandPoolCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				info.queueFamilyIndex = VulkanRHI::get()->getVulkanDevice()->findQueueFamilies().graphicsFamily;
				vkCheck(vkCreateCommandPool(VulkanRHI::get()->getDevice(), &info, nullptr, &m_gpuResource.commandPools[i]));
			}
			
			// Command buffer
			{
				VkCommandBufferAllocateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				info.commandPool = m_gpuResource.commandPools[i];
				info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				info.commandBufferCount = 1;
				vkCheck(vkAllocateCommandBuffers(VulkanRHI::get()->getDevice(), &info, &m_gpuResource.commandBuffers[i]));
			}
		}
	}
}

void ImguiPass::renderpassRelease()
{
	vkDestroyRenderPass(VulkanRHI::get()->getDevice(),m_gpuResource.renderPass,nullptr);

	auto backBufferSize = m_gpuResource.framebuffers.size();
	for (uint32_t i = 0; i < backBufferSize; i++)
	{
		vkFreeCommandBuffers(VulkanRHI::get()->getDevice(),m_gpuResource.commandPools[i],1,&m_gpuResource.commandBuffers[i]);
		vkDestroyCommandPool(VulkanRHI::get()->getDevice(),m_gpuResource.commandPools[i],nullptr);
		vkDestroyFramebuffer(VulkanRHI::get()->getDevice(),m_gpuResource.framebuffers[i],nullptr);
	}

	m_gpuResource.framebuffers.resize(0);
	m_gpuResource.commandPools.resize(0);
	m_gpuResource.commandBuffers.resize(0);
}

void ImguiPass::newFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImguiPass::initImgui()
{
	if(m_init)
	{
		return;
	}

	// 创建ImguiPass专用的描述符池
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		vkCheck(vkCreateDescriptorPool(VulkanRHI::get()->getDevice(), &pool_info, nullptr, &m_gpuResource.descriptorPool));
	}

	renderpassBuild();
	VulkanRHI::get()->addBeforeSwapchainRebuildCallback("ImguiPassRelease",[&]() {
		renderpassRelease();
	});
	VulkanRHI::get()->addAfterSwapchainRebuildCallback("ImguiPassRebuild",[&]() {
		renderpassBuild();
	});

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); 
	(void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // 开启键盘控制
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // 开启Docking
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // 开启游戏手柄控制
	
	// 在我的机器上多视口会有大量的Vulkan错误
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // 开启多视口控制

	setupStyle();
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// GLFW回调设置
	ImGui_ImplGlfw_InitForVulkan(g_windowData.window, true);

	ImGui_ImplVulkan_InitInfo vkInitInfo{ };
	setupVulkanInitInfo(&vkInitInfo,m_gpuResource.descriptorPool);
	ImGui_ImplVulkan_Init(&vkInitInfo,m_gpuResource.renderPass);

	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	const auto font_size    = 15.0f;
	const auto font_scale   = 1.0f;
	
	io.Fonts->AddFontFromFileTTF((s_fontsDir + std::string("Deng.ttf")).c_str(), font_size, NULL, io.Fonts->GetGlyphRangesChineseFull());
	io.FontGlobalScale = font_scale;

	// 上传字体
	{
		VkCommandPool command_pool = m_gpuResource.commandPools[0];
		VkCommandBuffer command_buffer = m_gpuResource.commandBuffers[0];
		vkCheck(vkResetCommandPool(VulkanRHI::get()->getDevice(), command_pool, 0));
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck(vkBeginCommandBuffer(command_buffer, &begin_info));
		ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &command_buffer;
		vkCheck(vkEndCommandBuffer(command_buffer));
		vkCheck(vkQueueSubmit(vkInitInfo.Queue, 1, &end_info, VK_NULL_HANDLE));
		vkCheck(vkDeviceWaitIdle(vkInitInfo.Device));
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	m_init = true;
}

void ImguiPass::renderFrame(uint32 backBufferIndex)
{
	ImGui::Render();
	ImDrawData* main_draw_data = ImGui::GetDrawData();
	{
		vkCheck(vkResetCommandPool(VulkanRHI::get()->getDevice(), m_gpuResource.commandPools[backBufferIndex], 0));
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck(vkBeginCommandBuffer(m_gpuResource.commandBuffers[backBufferIndex], &info));
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = m_gpuResource.renderPass;
		info.framebuffer = m_gpuResource.framebuffers[backBufferIndex];
		info.renderArea.extent.width = VulkanRHI::get()->getSwapchainExtent().width;
		info.renderArea.extent.height = VulkanRHI::get()->getSwapchainExtent().height;
		info.clearValueCount = 1;

		VkClearValue clearColor{ };
		clearColor.color.float32[0] = m_clearColor.x * m_clearColor.w;
		clearColor.color.float32[1] = m_clearColor.y * m_clearColor.w;
		clearColor.color.float32[2] = m_clearColor.z * m_clearColor.w;
		clearColor.color.float32[3] = m_clearColor.w;
		info.pClearValues = &clearColor;
		vkCmdBeginRenderPass(m_gpuResource.commandBuffers[backBufferIndex], &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	ImGui_ImplVulkan_RenderDrawData(main_draw_data, m_gpuResource.commandBuffers[backBufferIndex]);

	vkCmdEndRenderPass(m_gpuResource.commandBuffers[backBufferIndex]);
	vkEndCommandBuffer(m_gpuResource.commandBuffers[backBufferIndex]);
}

void ImguiPass::updateAfterSubmit()
{
	ImGuiIO& io = ImGui::GetIO(); 
	(void)io;
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImguiPass::release()
{
	if(!m_init)
	{
		return;
	}

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	renderpassRelease();
	vkDestroyDescriptorPool(VulkanRHI::get()->getDevice(), m_gpuResource.descriptorPool, nullptr);

	m_init = false;
}

}