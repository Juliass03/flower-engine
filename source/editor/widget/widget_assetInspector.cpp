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
	auto cacheInfo =  MaterialInfoCache::s_cache->getMaterialInfoCache(path);
	
	ImGui::Text(cacheInfo->shaderName.c_str());

	for(auto& texture : cacheInfo->textures)
	{
		ImGui::Text(texture.first.c_str());
		if(ImGui::Button(texture.second.second.c_str()))
		{
			EditorAsset::get().inspectorRequireSkip = true;
			EditorAsset::get().inspectorClickAssetPath = texture.second.second;
		}
	}
}
