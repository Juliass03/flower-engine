#include "widget_viewport.h"
#include "../../imgui/imgui.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>
#include "../../engine/asset_system/asset_system.h"

using namespace engine;
using namespace engine::asset_system;

WidgetViewport::WidgetViewport(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"³¡¾°ÊÓÍ¼";
}

void WidgetViewport::onVisibleTick(size_t i)
{
	static int g_width = 0, g_height = 0;
	glfwGetFramebufferSize(g_windowData.window, &g_width, &g_height);
	bool mini_window = (g_width==0||g_height==0);
	
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if(!mini_window) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));

	if (!ImGui::Begin(m_title.c_str(), &m_visible, ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::End();
		return;
	}
	
	float width     = ImGui::GetContentRegionAvail().x;
	float height    = ImGui::GetContentRegionAvail().y;
	
	// 

	m_renderer->UpdateScreenSize((uint32)width,(uint32)height);
	m_cacheImageInfo.setIcon(m_renderer->getRenderScene().getSceneTextures().getLDRSceneColor());
	m_cacheImageInfo.setSampler(VulkanRHI::get()->getPointClampSampler());
	
	ImGui::Image(m_cacheImageInfo.getId(uint32_t(i)),ImVec2(width,height));
	
	//ImGui::Image((ImTextureID)EngineAsset::get()->iconFile.getId(),ImVec2(width,height));
	ImGui::End();
	if(!mini_window)ImGui::PopStyleVar(1);
}

WidgetViewport::~WidgetViewport()
{

}
