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
	std::string hoverAssetPath;     
	std::string leftClickAssetPath = "";
	std::string rightClickAssetPath; 

	std::string workingFoler;

	static EditorAsset& get()
	{
		static EditorAsset instance;
		return instance;
	}

	std::string inspectorClickAssetPath = "";
	bool inspectorRequireSkip = false;

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
	void emptyAreaClickPopupMenu();

	engine::Ref<engine::asset_system::AssetSystem> m_assetSystem;
	std::filesystem::path m_projectDirectory;

	std::vector<CacheEntryInfo> m_currentFolderCacheEntry;
	std::atomic<bool> bNeedScan = false;

	ImGuiTextFilter m_searchFilter;
	std::chrono::duration<double,std::milli> m_timeSinceLastClick;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastClickTime;

	bool m_isHoveringItem;
	std::string m_hoverItemName;
	std::string buf; // ”√”⁄InputText

	std::string m_renameingFile{};
	bool bRenaming = false;

	bool m_isHoveringWindow;
};