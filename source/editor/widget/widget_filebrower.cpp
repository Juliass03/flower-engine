#include "widget_filebrower.h"
#include "../../engine/asset_system/asset_system.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"

using namespace engine;

EFileBrowserAction EditorFileBrowser::g_action = EFileBrowserAction::SaveScene;
std::unordered_map<int32_t,std::function<void(std::string selectName)>> EditorFileBrowser::g_callbackMap = {};

ImGuiFileBrowserFlags f_folderBrowerFlags = ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_NoStatusBar;

ImGui::FileBrowser g_fileDialogInstance = ImGui::FileBrowser(f_folderBrowerFlags);

std::vector<std::string> getActionFilterString()
{
	switch(EditorFileBrowser::g_action)
	{
	case EFileBrowserAction::SaveScene:
	case EFileBrowserAction::SaveScene_NewScene:
	case EFileBrowserAction::LoadScene:
	case EFileBrowserAction::SaveScene_LoadScene:
	case EFileBrowserAction::SaveScene_Close:
		return {".flower"};
	case EFileBrowserAction::PMXMesh_Select:
		return {".pmx"};
	}

	return {".flower"};
}

WidgetFileBrowser::WidgetFileBrowser(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"FileBrowser";
	g_fileDialogInstance.SetTypeFilters({".flower"});

	m_sceneManager = engine->getRuntimeModule<SceneManager>();
}

void WidgetFileBrowser::onVisibleTick(size_t)
{
	auto saveScene = [&](std::string path)
	{
		if(m_sceneManager->saveScene(path))
		{
			LOG_INFO("Saved project path {0}.",path);
		}
	};

	auto loadScene = [&](std::string path)
	{
		m_sceneManager->unloadScene();
		if(m_sceneManager->loadScene(path))
		{
			LOG_INFO("Loaded project path {0}.",path);
		}
	};

	auto selectPMX = [&](std::string path)
	{
		if(auto callback = EditorFileBrowser::g_callbackMap[(int32_t)EFileBrowserAction::PMXMesh_Select])
		{
			callback(path);
		}
	};

	g_fileDialogInstance.SetTypeFilters(getActionFilterString());
	g_fileDialogInstance.Display();
	
	if(g_fileDialogInstance.HasSelected())
	{
		std::string path = g_fileDialogInstance.GetSelected().string();

		if(EditorFileBrowser::g_action == EFileBrowserAction::SaveScene)
		{
			saveScene(path);
		}
		else if(EditorFileBrowser::g_action == EFileBrowserAction::LoadScene)
		{
			loadScene(path);
		}
		else if(EditorFileBrowser::g_action == EFileBrowserAction::SaveScene_LoadScene)
		{
			// todo:
		}
		else if(EditorFileBrowser::g_action == EFileBrowserAction::SaveScene_Close)
		{
			// todo:
		}
		else if(EditorFileBrowser::g_action == EFileBrowserAction::PMXMesh_Select)
		{
			selectPMX(path);
		}

		g_fileDialogInstance.ClearSelected();
	}
}

WidgetFileBrowser::~WidgetFileBrowser()
{


}
