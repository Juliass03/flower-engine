#pragma once
#include "widget.h"
#include "../../engine/asset_system/asset_system.h"
#include <filesystem>
class WidgetAsset : public Widget
{
public:
	WidgetAsset(engine::Ref<engine::Engine> engine);

	virtual void onVisibleTick() override;
	~WidgetAsset();

private:
	engine::Ref<engine::asset_system::AssetSystem> m_assetSystem;
	std::filesystem::path m_projectDirectory;
};