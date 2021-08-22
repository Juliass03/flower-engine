#include "widget_asset.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_vulkan.h"
#include "../../engine/core/timer.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>
#include "../../engine/core/file_system.h"
#include <stack>
#include <thread>
#include <algorithm>

using namespace engine;
using namespace engine::asset_system;

struct CacheEntryInfoCompare
{
	bool operator()(const CacheEntryInfo& a,const CacheEntryInfo& b)
	{
		if((a.bFolder && b.bFolder) || (!a.bFolder && !b.bFolder))
		{
			return a.filenameString < b.filenameString;
		}
		return a.bFolder > b.bFolder;
	}
}myCacheEntryInfoCompare;

WidgetAsset::WidgetAsset(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"资源管理器";
	m_projectDirectory = s_projectDirRaw;
	m_assetSystem = engine->getRuntimeModule<AssetSystem>();

	bNeedScan = true;
}

void WidgetAsset::onVisibleTick()
{
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	const bool bAtProjectDir = 
			(m_projectDirectory != std::filesystem::path(s_projectDir)) 
		&&  (m_projectDirectory != std::filesystem::path(s_projectDirRaw));

	if ( bAtProjectDir)
	{
		if (ImGui::Button("<"))
		{
			m_projectDirectory = m_projectDirectory.parent_path();
			bNeedScan = true;
		}
	}
	else
	{
		ImGui::ButtonStyle("<");
	}

	std::stack<std::pair<std::filesystem::path,std::string>> cacheFolderPath;
	std::filesystem::path tempDir = m_projectDirectory;

	while(true)
	{
		const bool bCurrentNotAtProjectDir = 
			(tempDir != std::filesystem::path(s_projectDir)) 
			&&  (tempDir != std::filesystem::path(s_projectDirRaw));

		auto strings = tempDir.string();
		cacheFolderPath.push({tempDir, FileSystem::getFolderRawName(strings)});

		if(!bCurrentNotAtProjectDir)
		{
			break;
		}

		tempDir = tempDir.parent_path();
	}

	while(!cacheFolderPath.empty())
	{
		std::string folder = cacheFolderPath.top().second;
		ImGui::SameLine();
		if(ImGui::Button(folder.c_str()))
		{
			m_projectDirectory = cacheFolderPath.top().first;
			bNeedScan = true;
		}
		ImGui::SameLine();
		ImGui::Text("/");

		cacheFolderPath.pop();
	}
	
	static float padding = 16.0f;
	static int thumbnailSize = 50;
	float cellSize = thumbnailSize + padding;

	ImGui::SameLine();
	float LengthOfSlider = 120.0f;
	float textStartPositionX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - LengthOfSlider;
	ImGui::SetCursorPosX(textStartPositionX);
	ImGui::SetNextItemWidth(LengthOfSlider);
	ImGui::SliderInt("ThumbnailSize", &thumbnailSize, 30, 100);
	ImGui::Separator();

	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = (int)(panelWidth / cellSize);
	if (columnCount < 1)
		columnCount = 1;

	ImGui::Columns(columnCount, 0, false);

	// Lazy scan
	// 通常不会每帧调用
	auto ScanFolder = [&]()
	{
		m_currentFolderCacheEntry.clear();
		for (auto& directoryEntry : std::filesystem::directory_iterator(m_projectDirectory))
		{
			CacheEntryInfo cacheInfo{};
			cacheInfo.bFolder = directoryEntry.is_directory();
			auto relativePath = std::filesystem::relative(directoryEntry.path(), s_projectDir);
			cacheInfo.filenameString = relativePath.filename().string();
			cacheInfo.itemPath = relativePath.c_str();
			cacheInfo.path = directoryEntry.path().filename();
			m_currentFolderCacheEntry.push_back(cacheInfo);
		}

		std::sort(m_currentFolderCacheEntry.begin(),m_currentFolderCacheEntry.end(),myCacheEntryInfoCompare);
	};

	if(bNeedScan)
	{
		ScanFolder();
		bNeedScan = false;
	}

	for(auto& cacheInfo : m_currentFolderCacheEntry)
	{
		ImGui::PushID(cacheInfo.filenameString.c_str());
		auto& icon = cacheInfo.bFolder ? EngineAsset::get()->iconFolder : EngineAsset::get()->iconFile;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		ImGui::ImageButton((ImTextureID)icon.getId(), { (float)thumbnailSize, (float)thumbnailSize }, { 0, 1 }, { 1, 0 });

		if (ImGui::BeginDragDropSource())
		{
			const wchar_t* itemPath = cacheInfo.itemPath;
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
			ImGui::EndDragDropSource();
		}

		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if(cacheInfo.bFolder)
			{
				m_projectDirectory /= cacheInfo.path;
				bNeedScan = true;
			}
		}

		auto RawName = FileSystem::getFileRawName(cacheInfo.filenameString);
		float tipStartPositionX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() * 0.5f - ImGui::CalcTextSize((RawName + " ").c_str()).x*0.5f;
		ImGui::SetCursorPosX(tipStartPositionX);
		ImGui::Text(RawName.c_str());

		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::End();
}

WidgetAsset::~WidgetAsset()
{

}