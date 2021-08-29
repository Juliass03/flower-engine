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

using namespace engine;

void showDockSpace(bool* p_open)
{
    // See imguidemo.cpp
    // imgui示例中的Dockspace.
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    else
    {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("MyDockSpaceDemo", p_open, window_flags);
    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // Submit the DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(u8"开始"))
        {
            // Disabling fullscreen would allow the window to be moved to the front of other windows,
            // which we can't undo at the moment without finer window depth/z control.
            ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
            ImGui::MenuItem("Padding", NULL, &opt_padding);
            ImGui::Separator();

            if (ImGui::MenuItem("Flag: NoSplit",                "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0))                 { dockspace_flags ^= ImGuiDockNodeFlags_NoSplit; }
            if (ImGui::MenuItem("Flag: NoResize",               "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))                { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
            if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0))  { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode; }
            if (ImGui::MenuItem("Flag: AutoHideTabBar",         "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))          { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
            if (ImGui::MenuItem("Flag: PassthruCentralNode",    "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
            ImGui::Separator();

            if (ImGui::MenuItem(u8"关闭", NULL, false, p_open != NULL))
                *p_open = false;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

void Editor::init()
{
	g_engineLoop.init();
	auto engine = g_engineLoop.getEngine();
    
    
    engine->getRuntimeModule<Renderer>()->addImguiFunction("dockSpace",[&](size_t){
        showDockSpace(g_engineLoop.getRun());
    });

    //
    m_widgets.push_back(new WidgetConsole(engine));
    m_widgets.push_back(new WidgetViewport(engine));
    m_widgets.push_back(new WidgetAsset(engine));
    m_widgets.push_back(new WidgetDownbar(engine));
    m_widgets.push_back(new WidgetHierarchy(engine));
    m_widgets.push_back(new WidgetImguiDemo(engine));
    m_widgets.push_back(new WidgetDetail(engine)); // Detail应该放在Hierarchy后
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