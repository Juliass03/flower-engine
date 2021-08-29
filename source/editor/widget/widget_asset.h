#pragma once
#include "widget.h"
#include "../../engine/asset_system/asset_system.h"
#include <filesystem>
#include <stack>
#include <atomic>
#include <string>

struct CacheEntryInfo
{
	bool bFolder;
	std::string filenameString;
	const wchar_t* itemPath;
	std::filesystem::path path;
};

class WidgetAsset : public Widget
{
public:
	WidgetAsset(engine::Ref<engine::Engine> engine);

	virtual void onVisibleTick(size_t) override;
	~WidgetAsset();

private:
	engine::Ref<engine::asset_system::AssetSystem> m_assetSystem;
	std::filesystem::path m_projectDirectory;

	std::vector<CacheEntryInfo> m_currentFolderCacheEntry;
	std::atomic<bool> bNeedScan = false;
};