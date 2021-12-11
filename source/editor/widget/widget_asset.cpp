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
#include "../../imgui/imgui_internal.h"
#include "../../engine/renderer/material.h"
#include "../../engine/renderer/texture.h"

using namespace engine;
using namespace engine::asset_system;

const char* g_flushCallbackName = "FlushAssetWidget";

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
	m_title = u8"Asset";
	m_projectDirectory = s_projectDirRaw;
	m_assetSystem = engine->getRuntimeModule<AssetSystem>();

	bNeedScan = true;
	m_assetSystem->registerOnAssetFolderDirtyCallBack(g_flushCallbackName,[this](){
		bNeedScan = true;
	});
}

void WidgetAsset::scanFolder()
{
	m_currentFolderCacheEntry.clear();
	std::filesystem::path fullPath = std::filesystem::absolute(m_projectDirectory);
	for (auto& directoryEntry : std::filesystem::directory_iterator(fullPath))
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

void WidgetAsset::onVisibleTick(size_t)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	m_isHoveringWindow = false;
	m_isHoveringItem = false;

	bool forceHightlightAsset = false;

	if(EditorAsset::get().inspectorRequireSkip)
	{
		EditorAsset::get().inspectorRequireSkip = false;

		if(FileSystem::startWith(EditorAsset::get().inspectorClickAssetPath,"./project/"))
		{
			m_projectDirectory = FileSystem::getFolderName(EditorAsset::get().inspectorClickAssetPath);
		}
		
		forceHightlightAsset = true;
		bNeedScan = true;
	}

	const bool bAtProjectDir = 
			(m_projectDirectory != std::filesystem::path(s_projectDir)) 
		&&  (m_projectDirectory != std::filesystem::path(s_projectDirRaw));
	if ( bAtProjectDir)
	{
		// 返回主项目目录
		if (ImGui::ImageButton(EngineAsset::get()->iconBack->getId(),
			ImVec2(ImGuiStyleEx::imageButtonSize,ImGuiStyleEx::imageButtonSize)
		))
		{
			m_projectDirectory = m_projectDirectory.parent_path();
			bNeedScan = true;
		}
	}
	else
	{
		ImGui::ImageButton(EngineAsset::get()->iconHome->getId(),
			ImVec2(ImGuiStyleEx::imageButtonSize,ImGuiStyleEx::imageButtonSize),ImVec2(0,1),ImVec2(1,0));
	}
	ImGui::SameLine();
	if(ImGui::ImageButton(EngineAsset::get()->iconFlash->getId(),
		ImVec2(ImGuiStyleEx::imageButtonSize,ImGuiStyleEx::imageButtonSize),ImVec2(0,1),ImVec2(1,0)))
	{
		bNeedScan = true;
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
	
	static int thumbnailSize = 80;

	ImGui::SameLine();
	float LengthOfSlider = 120.0f;
	float textStartPositionX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - LengthOfSlider;
	ImGui::SetCursorPosX(textStartPositionX);
	ImGui::SetNextItemWidth(LengthOfSlider);
	ImGui::SliderInt("ThumbnailSize", &thumbnailSize, 30, 100);
	ImGui::Separator();

	std::string searchName = u8"Keyword";
	const float label_width = ImGui::CalcTextSize(searchName.c_str(), nullptr, true).x;
	m_searchFilter.Draw(searchName.c_str(),180.0f);
	ImGui::Separator();

	if(bNeedScan)
	{
		scanFolder();
		bNeedScan = false;
	}

	uint32 displayCount = 0;

	// 开始绘制文件节点
	{
		bool bNewline = true;
		
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg,IM_COL32(0,0,0,50));

		const float buttomOffset = ImGui::GetTextLineHeight() * 1.2f;
		const auto contentWidth = ImGui::GetContentRegionAvail().x;
		const auto contentHeight = ImGui::GetContentRegionAvail().y - buttomOffset;
		float penXMin = 0.0f;
		float penX  = 0.0f;
		ImGuiStyle& style = ImGui::GetStyle();
		ImRect rect_button;
		ImRect rect_label;
		const float labelHeight = GImGui->FontSize * 1.2f;

		if(ImGui::BeginChild("##ContentRegion",ImVec2(contentWidth,contentHeight),true))
		{
			m_isHoveringWindow = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByPopup|
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ?
				true : m_isHoveringWindow;

			float offset = ImGui::GetStyle().ItemSpacing.x;
			penXMin = ImGui::GetCursorPosX() + offset;
			ImGui::SetCursorPosX(penXMin);

			for(auto& cacheInfo : m_currentFolderCacheEntry)
			{
				// 我们跳过非指定后缀的文件
				if(!cacheInfo.bFolder && !(
					FileSystem::endWith(cacheInfo.filenameString,".material") ||
					FileSystem::endWith(cacheInfo.filenameString,".flower") ||
					FileSystem::endWith(cacheInfo.filenameString,".texture") ||
					FileSystem::endWith(cacheInfo.filenameString,".mesh")
				))
				{
					continue;
				}


				if(!m_searchFilter.PassFilter(cacheInfo.filenameString.c_str()))
				{
					continue;
				}

				EngineAsset::IconInfo* icon = EngineAsset::get()->iconFile;
				if(cacheInfo.bFolder)
				{
					icon = EngineAsset::get()->iconFolder;			
				}
				else if(FileSystem::endWith(cacheInfo.filenameString,".flower"))
				{
					icon = EngineAsset::get()->iconProject;
				}
				else if(FileSystem::endWith(cacheInfo.filenameString,".material"))
				{
					icon = EngineAsset::get()->iconMaterial;
				}
				else if(FileSystem::endWith(cacheInfo.filenameString,".mesh"))
				{
					icon = EngineAsset::get()->iconMesh;
				}
				else if(FileSystem::endWith(cacheInfo.filenameString,".texture"))
				{
					std::string engineFolderPathFormat = FileSystem::toCommonPath(m_projectDirectory);
					auto* texIcon = EngineAsset::get()->getIconInfo(engineFolderPathFormat + "/"+cacheInfo.filenameString);

					if(texIcon->icon)
					{
						icon = texIcon;
					}
					else
					{
						icon = EngineAsset::get()->iconTexture;
					}
				}

				displayCount++;

				if(bNewline)
				{
					ImGui::BeginGroup();
					bNewline = false;
				}

				ImGui::BeginGroup();
				{
					{
						rect_button = ImRect(
							ImGui::GetCursorScreenPos().x,
							ImGui::GetCursorScreenPos().y,
							ImGui::GetCursorScreenPos().x + thumbnailSize,
							ImGui::GetCursorScreenPos().y + thumbnailSize
						);

						rect_label = ImRect(
							rect_button.Min.x,
							rect_button.Max.y - labelHeight - style.FramePadding.y,
							rect_button.Max.x,
							rect_button.Max.y
						);
					}

					{
						static const float shadow_thickness = 2.0f;
						ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_BorderShadow];
						ImGui::GetWindowDrawList()->AddRectFilled(rect_button.Min,ImVec2(rect_label.Max.x+shadow_thickness,rect_label.Max.y+shadow_thickness),IM_COL32(color.x*255,color.y*255,color.z*255,color.w*255));
					}

					{
						ImGui::PushID(cacheInfo.filenameString.c_str());
						ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0,0,0,0));

						if(EditorAsset::get().leftClickAssetPath==cacheInfo.filenameString)
						{
							ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.2f,1.0f,0.2f,0.1f));
						}
						else
						{
							ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(1.0f,1.0f,1.0f,0.05f));
						}

						if(ImGui::Button("##dummy",ImVec2((float)thumbnailSize,(float)thumbnailSize)))
						{
							const auto now = std::chrono::high_resolution_clock::now();
							m_timeSinceLastClick = now-m_lastClickTime;
							m_lastClickTime = now;
							const bool is_single_click = m_timeSinceLastClick.count()>500;

							// 双击
							if(!is_single_click)
							{
								if(cacheInfo.bFolder)
								{
									m_projectDirectory /= cacheInfo.path;
									bNeedScan = true;
								}
							}
							// 单击
							else if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))
							{
								LOG_INFO("Select {0}。",cacheInfo.filenameString.c_str());
							}

							if(!cacheInfo.bFolder)
							{
								EditorAsset::get().leftClickAssetPath = cacheInfo.filenameString;
								EditorAsset::get().workingFoler = m_projectDirectory.string() + "/"; 
							}
						}

						{
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
							{
								m_isHoveringItem = true;
								m_hoverItemName = cacheInfo.filenameString;
							}

							// 右键点击菜单
							if(ImGui::IsItemClicked(1) && m_isHoveringWindow && m_isHoveringItem && !cacheInfo.bFolder)
							{
								ImGui::OpenPopup("##ItemRightClickContextMenu");
							}

							if(ImGui::BeginPopup("##ItemRightClickContextMenu"))
							{
								ImGui::Separator();
								if (ImGui::MenuItem(u8"Rename"))
								{
									m_renameingFile = cacheInfo.filenameString;
									bRenaming = true;

									buf = FileSystem::getFileNameWithoutSuffix(m_renameingFile);
								}
								ImGui::EndPopup();
							}
						}

						// 图片
						{
							ImVec2 imageSizeMax = ImVec2(rect_button.Max.x-rect_button.Min.x-style.FramePadding.x*2.0f,rect_button.Max.y-rect_button.Min.y-style.FramePadding.y-labelHeight-5.0f);
							ImVec2 imageSize = ImVec2(static_cast<float>(icon->icon->getInfo().extent.width),static_cast<float>(icon->icon->getInfo().extent.height));
							ImVec2 imageSizeDelta = ImVec2(0.0f,0.0f);

							{
								if(imageSize.x!=imageSizeMax.x)
								{
									float scale = imageSizeMax.x/imageSize.x;
									imageSize.x = imageSizeMax.x;
									imageSize.y = imageSize.y*scale;
								}
								if(imageSize.y!=imageSizeMax.y)
								{
									float scale = imageSizeMax.y/imageSize.y;
									imageSize.x = imageSize.x*scale;
									imageSize.y = imageSizeMax.y;
								}

								imageSizeDelta.x = imageSizeMax.x-imageSize.x;
								imageSizeDelta.y = imageSizeMax.y-imageSize.y;
							}

							ImGui::SetCursorScreenPos(ImVec2(rect_button.Min.x+style.FramePadding.x+imageSizeDelta.x*0.5f,rect_button.Min.y+style.FramePadding.y+imageSizeDelta.y*0.5f));
							ImGui::Image((ImTextureID)icon->getId(),imageSize,ImVec2(0,1),ImVec2(1,0));
						}

						ImGui::PopStyleColor(2);
						ImGui::PopID();
					}

					// 标签
					{
						const char* labelText = cacheInfo.filenameString.c_str();
						const ImVec2 labelSize = ImGui::CalcTextSize(labelText,nullptr,true);
						const float textOffset = 3.0f;

						ImGui::GetWindowDrawList()->AddRectFilled(rect_label.Min,rect_label.Max,IM_COL32(51,51,51,190));

						if(labelSize.x<=thumbnailSize&&labelSize.y<=thumbnailSize)
						{
							ImGui::SetCursorScreenPos(ImVec2(
								rect_label.Min.x + (thumbnailSize - ImGui::CalcTextSize((cacheInfo.filenameString).c_str()).x) * 0.5f,
								rect_label.Min.y + textOffset
							));

							ImGui::TextUnformatted(labelText);
						}
						else
						{
							ImGui::SetCursorScreenPos(ImVec2(
								rect_label.Min.x + textOffset,
								rect_label.Min.y + textOffset
							));
							ImGui::RenderTextClipped(rect_label.Min,rect_label.Max,labelText,nullptr,&labelSize,ImVec2(0,0.6f),&rect_label);
						}
					}
					ImGui::EndGroup();
				}

				if(forceHightlightAsset)
				{
					if(FileSystem::getFileName(EditorAsset::get().inspectorClickAssetPath)==cacheInfo.filenameString)
					{
						EditorAsset::get().leftClickAssetPath = cacheInfo.filenameString;
						EditorAsset::get().workingFoler = m_projectDirectory.string(); 
					}
				}

				// 判断换行
				penX += thumbnailSize+ImGui::GetStyle().ItemSpacing.x;
				if(penX>=contentWidth-thumbnailSize)
				{
					ImGui::EndGroup();
					penX = penXMin;
					ImGui::SetCursorPosX(penX);
					bNewline = true;
				}
				else
				{
					ImGui::SameLine();
				}
			}

			if(!bNewline)
			{
				ImGui::EndGroup();
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	{
		const float offsetBottom = ImGui::GetTextLineHeight() * 1.2f;
		ImGui::SetCursorPosY(ImGui::GetWindowSize().y - offsetBottom);
		ImGui::Text(u8"%d Items", displayCount);
	}

	if(ImGui::IsMouseClicked(0) && !m_isHoveringItem && m_isHoveringWindow)
	{
		EditorAsset::get().leftClickAssetPath = "";
	}
	
	emptyAreaClickPopupMenu();

	if (bRenaming)
	{
		ImGui::OpenPopup("##ItemRenameAsset");
		bRenaming = false;
	}

	if (ImGui::BeginPopup("##ItemRenameAsset"))
	{
		std::string suffix = FileSystem::getFileSuffixName(m_renameingFile);
		
		ImGui::Text(u8"New Name");

		ImGui::SetNextItemWidth(80.0f);
		ImGui::InputText("##edit", &buf);
		if (ImGui::Button(u8"Ok")) 
		{ 
			if(buf != FileSystem::getFolderRawName(m_renameingFile))
			{
				auto projectPath = m_projectDirectory.string() + "/";
				auto finalName = FileSystem::getFolderName(m_renameingFile) + buf + suffix;
				std::filesystem::rename( projectPath + m_renameingFile,projectPath + finalName);
				bNeedScan = true;
			}
			
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

WidgetAsset::~WidgetAsset()
{
	m_assetSystem->unregisterOnAssetFolderDirtyCallBack(g_flushCallbackName);
}

void WidgetAsset::emptyAreaClickPopupMenu()
{
	if(ImGui::IsMouseClicked(1) && m_isHoveringWindow && !m_isHoveringItem)
	{
		ImGui::OpenPopup("##Content_ContextMenu");
	}

	if(!ImGui::BeginPopup("##Content_ContextMenu"))
		return;

	if(ImGui::MenuItem(u8"New Material"))
	{
		MaterialLibrary::get()->createEmptyMaterialAsset(m_projectDirectory.string() + "/newMaterial");
		bNeedScan = true;
	}

	ImGui::EndPopup();
}
