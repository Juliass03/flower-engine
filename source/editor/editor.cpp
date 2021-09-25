#include "editor.h"
#include "../engine/launch/launch_engine_loop.h"
#include "../engine/renderer/renderer.h"
#include "widget/widget.h"
#include "widget/widget_imgui_demo.h"
#include "widget/widget_console.h"
#include "widget/widget_downbar.h"
#include "widget/widget_asset.h"
#include "widget/widget_viewport.h"
#include "widget/widget_hierarchy.h"
#include "widget/widget_detail.h"
#include "widget/dockspace.h"
#include "widget/widget_filebrower.h"

using namespace engine;

void Editor::init()
{
	g_engineLoop.init();
	auto engine = g_engineLoop.getEngine();

    //
	m_widgets.push_back(new DockSpace(engine));
    m_widgets.push_back(new WidgetConsole(engine));
    m_widgets.push_back(new WidgetViewport(engine));
    m_widgets.push_back(new WidgetAsset(engine));
	m_widgets.push_back(new WidgetImguiDemo(engine));
    m_widgets.push_back(new WidgetDownbar(engine));
    m_widgets.push_back(new WidgetHierarchy(engine));
    m_widgets.push_back(new WidgetDetail(engine)); // Detail应该放在Hierarchy后

    // NOTE: file brower 放到最后
    m_widgets.push_back(new WidgetFileBrowser(engine));
}

void Editor::loop()
{
	// Register ui functions.
	for(auto& widget : m_widgets)
	{
		std::string name = widget->getTile();
		widget->getRenderer()->addImguiFunction(name,[&](size_t i)
		{
			widget->tick(i);
		});
	}
	g_engineLoop.guardedMain();
}

void Editor::release()
{
	for(auto& widget:m_widgets)
	{
		std::string name = widget->getTile();
		widget->getRenderer()->removeImguiFunction(name);
		widget->release();
	}

	g_engineLoop.release();
	for(auto& i : m_widgets)
	{
		delete i;
	}
}