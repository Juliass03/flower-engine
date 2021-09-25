#pragma once
#include "widget.h"
#include "../../imgui/imgui.h"
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

class EditorAsset
{
public:
	std::string hoverAssetPath;       // 上一次选择的节点
	std::string leftClickAssetPath;   // 左键点击按钮
	std::string rightClickAssetPath;  // 右键按钮，也是选中按钮

	static EditorAsset& get()
	{
		static EditorAsset instance;
		return instance;
	}

	std::string getSelectAsset() const { return rightClickAssetPath; }
};

class WidgetAsset : public Widget
{
public:
	WidgetAsset(engine::Ref<engine::Engine> engine);

	virtual void onVisibleTick(size_t) override;
	~WidgetAsset();

private:
	void scanFolder();
	void popupMenu();

	engine::Ref<engine::asset_system::AssetSystem> m_assetSystem;
	std::filesystem::path m_projectDirectory;

	std::vector<CacheEntryInfo> m_currentFolderCacheEntry;
	std::atomic<bool> bNeedScan = false;

	ImGuiTextFilter m_searchFilter;
	std::chrono::duration<double,std::milli> m_timeSinceLastClick;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastClickTime;
};