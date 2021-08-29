#include "widget.h"
#include "../../imgui/imgui_impl_vulkan.h"
#include "../../imgui/imgui_internal.h"
using namespace engine;

uint32_t Widget::s_widgetCount = 0;

Widget::Widget(Ref<Engine> engine)
{
	m_engine = engine;
	m_renderer = engine->getRuntimeModule<Renderer>();

	s_widgetCount ++;
}

void Widget::tick(size_t i)
{
	onTick(i);

	if(m_visible)
	{
		onVisibleTick(i);
	}
}

void Widget::release()
{
	onRelease();
}

ImTextureID ImageInfo::getId(uint32_t i)
{
	if(cacheId.size()==0)
	{
		cacheId.resize(VulkanRHI::get()->getSwapchainImageViews().size());
		for(size_t t = 0; t<cacheId.size(); t++)
		{
			cacheId[t] = nullptr;
		}
	}

	if(cacheId[i] != nullptr)
	{
		ImGui_ImlVulkan_FreeSet(cacheId[i]);
	}

	cacheId[i] = ImGui_ImplVulkan_AddTexture(sampler,icon->getImageView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return cacheId[i];
}

void Drawer::vector3(const std::string& label,glm::vec3& values,float resetValue,float columnWidth)
{
	ImGuiIO& io = ImGui::GetIO();
	auto boldFont = io.Fonts->Fonts[0];
	ImGui::PushID(label.c_str());
	
	float size = ImGui::CalcTextSize(label.c_str()).x;

	ImGui::Columns(2);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (size - ImGui::GetColumnOffset())*0.5f);
	ImGui::SetColumnWidth(0, size + ImGui::GetColumnOffset());

	ImGui::Text(label.c_str());

	
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

	const float sscale = 0.9f;
	ImVec2 buttonSize = { sscale * (lineHeight + 3.0f), sscale * lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.09f, 0.09f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.8f, 0.09f, 0.1f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("X", buttonSize))
		values.x = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.08f, 0.2f, 0.1f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.08f, 0.8f, 0.1f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Y", buttonSize))
		values.y = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	

	ImGui::SameLine();
	ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.08f, 0.09f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.08f, 0.09f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Z", buttonSize))
		values.z = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");

	
	ImVec2 buttonSizeR = buttonSize;
	buttonSizeR.x *= 0.7f;
	buttonSizeR.y *= 0.7f;
	ImGui::SameLine();
	if(ImGui::ImageButton(asset_system::EngineAsset::get()->iconFlash.getId(),buttonSizeR,
		ImVec2(0, 0),ImVec2(1,1),-1, ImVec4(0,0,0,0), ImVec4(1,1,0,1)))
	{
		values.x = resetValue;
		values.y = resetValue;
		values.z = resetValue;
	}

	ImGui::PopItemWidth();

	ImGui::PopStyleVar();
	
	ImGui::Columns(1);

	ImGui::PopID();

}
