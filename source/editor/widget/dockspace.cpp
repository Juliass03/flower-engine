#include "dockspace.h"
#include "../../imgui/imgui_impl_vulkan.h"
#include "../../imgui/imgui_internal.h"
#include "../../engine/core/core.h"
#include "widget_filebrower.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"
#include "../../engine/launch/launch_engine_loop.h"

using namespace engine;

DockSpace::DockSpace(engine::Ref<engine::Engine> engine)
    : Widget(engine)
{
    m_title = u8"MyDockSpace";

    m_sceneManager = engine->getRuntimeModule<SceneManager>();
}

void DockSpace::onVisibleTick(size_t)
{
    showDockSpace();
}

DockSpace::~DockSpace()
{
}

void DockSpace::showDockSpace()
{
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    {
        window_flags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("MyDockSpaceDemo", g_engineLoop.getRun(), window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(u8"File"))
        {
            if(ImGui::MenuItem(u8"New Scene",NULL,false,true))
            {
                LOG_INFO("NN");
            }

            if(ImGui::MenuItem(u8"Save Scene",NULL,false,true))
            {
                EditorFileBrowser::g_action = EFileBrowserAction::SaveScene;
                g_fileDialogInstance.Open();
            }

            if(ImGui::MenuItem(u8"Load Scene",NULL,false,true))
            {
                if(m_sceneManager->getActiveScene().isDirty())
                {
                    EditorFileBrowser::g_action = EFileBrowserAction::SaveScene_LoadScene;
                }
                else
                {
                    EditorFileBrowser::g_action = EFileBrowserAction::LoadScene;
                }

                g_fileDialogInstance.Open();
            }

            ImGui::Separator();

            if(ImGui::MenuItem(u8"Close",NULL,false,g_engineLoop.getRun()!=NULL))
            {
                if(m_sceneManager->getActiveScene().isDirty())
                {
                    EditorFileBrowser::g_action = EFileBrowserAction::SaveScene_Close;
                    g_fileDialogInstance.Open();
                }
                else
                {
                    *g_engineLoop.getRun() = false;
                }
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}
