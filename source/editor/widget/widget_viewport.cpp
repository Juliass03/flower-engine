#include "widget_viewport.h"
#include "../../imgui/imgui.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>

using namespace engine;

WidgetViewport::WidgetViewport(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = "Viewport";
}

void WidgetViewport::onVisibleTick()
{
	if(!m_renderer)
		return;

	
}

WidgetViewport::~WidgetViewport()
{

}
