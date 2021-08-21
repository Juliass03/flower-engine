#include "widget_imgui_demo.h"
#include "../../imgui/imgui.h"

WidgetImguiDemo::WidgetImguiDemo(engine::Ref<engine::Engine> engine)
: Widget(engine)
{
	m_title = "ImguiDemo";
}

void WidgetImguiDemo::onVisibleTick()
{
	ImGui::ShowDemoWindow(&m_visible);
}