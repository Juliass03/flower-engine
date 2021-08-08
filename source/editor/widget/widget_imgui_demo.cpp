#include "widget_imgui_demo.h"
#include "../../imgui/imgui.h"

WidgetImguiDemo::WidgetImguiDemo(engine::Ref<engine::Engine> engine)
: Widget(engine)
{

}

void WidgetImguiDemo::onVisibleTick()
{
	ImGui::ShowDemoWindow(&m_visible);
}