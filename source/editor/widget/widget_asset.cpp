#include "widget_asset.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_vulkan.h"
#include "../../engine/core/timer.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>
#include "../../engine/core/file_system.h"

using namespace engine;
using namespace engine::asset_system;

WidgetAsset::WidgetAsset(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"资源管理器";

	m_assetSystem = engine->getRuntimeModule<AssetSystem>();
}

void WidgetAsset::onVisibleTick()
{
	//ImGui::Begin("Content Browser");

	//if (m_projectDirectory != std::filesystem::path(s_projectDir))
	//{
	//	if (ImGui::Button("<-"))
	//	{
	//		m_projectDirectory = m_projectDirectory.parent_path();
	//	}
	//}

	//static float padding = 16.0f;
	//static float thumbnailSize = 128.0f;
	//float cellSize = thumbnailSize + padding;

	//float panelWidth = ImGui::GetContentRegionAvail().x;
	//int columnCount = (int)(panelWidth / cellSize);
	//if (columnCount < 1)
	//	columnCount = 1;

	//ImGui::Columns(columnCount, 0, false);

	//for (auto& directoryEntry : std::filesystem::directory_iterator(m_projectDirectory))
	//{
	//	const auto& path = directoryEntry.path();
	//	auto relativePath = std::filesystem::relative(path, s_projectDir);
	//	std::string filenameString = relativePath.filename().string();

	//	ImGui::PushID(filenameString.c_str());

	//	

	//	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	//	ImGui::ImageButton((ImTextureID)icon->GetRendererID(), { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 });

	//	if (ImGui::BeginDragDropSource())
	//	{
	//		const wchar_t* itemPath = relativePath.c_str();
	//		ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
	//		ImGui::EndDragDropSource();
	//	}

	//	ImGui::PopStyleColor();
	//	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	//	{
	//		if (directoryEntry.is_directory())
	//			m_projectDirectory /= path.filename();

	//	}
	//	ImGui::TextWrapped(filenameString.c_str());

	//	ImGui::NextColumn();

	//	ImGui::PopID();
	//}

	//ImGui::Columns(1);

	//ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
	//ImGui::SliderFloat("Padding", &padding, 0, 32);

	//// TODO: status bar
	//ImGui::End();

	
}

WidgetAsset::~WidgetAsset()
{

}