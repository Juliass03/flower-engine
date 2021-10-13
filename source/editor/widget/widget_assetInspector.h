#pragma once
#include <string>
#include "widget.h"

namespace engine
{
	class SceneManager;
	class SceneNode;
	class Scene;
	class Renderer;

	namespace asset_system
	{
		class AssetSystem;
	}
}

class WidgetAssetInspector : public Widget
{
public:
	WidgetAssetInspector(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetAssetInspector();

private:
	void inspectMaterial(const std::string& assetPath);

private:
	engine::Ref<engine::Renderer> m_renderer;
	engine::Ref<engine::SceneManager> m_sceneManager;
	engine::Ref<engine::asset_system::AssetSystem> m_assetSystem;
};