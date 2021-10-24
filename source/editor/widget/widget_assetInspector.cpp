#include "widget_assetInspector.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"
#include "../../engine/scene/components/transform.h"
#include "../../engine/renderer/material.h"
#include "../../engine/core/core.h"
#include "../../engine/renderer/mesh.h"
#include "../../engine/scene/components/staticmesh_renderer.h"
#include "widget_asset.h"
#include "../../engine/asset_system/asset_system.h"

using namespace engine;
using namespace engine::asset_system;

WidgetAssetInspector::WidgetAssetInspector(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"×ÊÔ´ÉèÖÃ";
	m_sceneManager = engine->getRuntimeModule<SceneManager>();
	m_assetSystem = engine->getRuntimeModule<asset_system::AssetSystem>();
	m_renderer = engine->getRuntimeModule<Renderer>();
}

void WidgetAssetInspector::onVisibleTick(size_t)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	std::string inspectAssetName = EditorAsset::get().leftClickAssetPath;
	if(FileSystem::endWith(inspectAssetName,".material"))
	{
		inspectMaterial(EditorAsset::get().workingFoler + inspectAssetName);
	}

	ImGui::End();
}

WidgetAssetInspector::~WidgetAssetInspector()
{

}

void WidgetAssetInspector::inspectMaterial(const std::string& path)
{
	Material* mat = MaterialLibrary::get()->getMaterial(path);

	ImGui::Text("BaseColor");
	if(ImGui::Button(mat->baseColorTexture.c_str()))
	{
		EditorAsset::get().inspectorRequireSkip = true;
		EditorAsset::get().inspectorClickAssetPath = mat->baseColorTexture;
	}

	ImGui::Text("Normal");
	if(ImGui::Button(mat->normalTexture.c_str()))
	{
		EditorAsset::get().inspectorRequireSkip = true;
		EditorAsset::get().inspectorClickAssetPath = mat->normalTexture;
	}

	ImGui::Text("Specular");
	if(ImGui::Button(mat->specularTexture.c_str()))
	{
		EditorAsset::get().inspectorRequireSkip = true;
		EditorAsset::get().inspectorClickAssetPath = mat->specularTexture;
	}

	ImGui::Text("Emissive");
	if(ImGui::Button(mat->emissiveTexture.c_str()))
	{
		EditorAsset::get().inspectorRequireSkip = true;
		EditorAsset::get().inspectorClickAssetPath = mat->emissiveTexture;
	}
}
