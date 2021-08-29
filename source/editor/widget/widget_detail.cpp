#include "widget_detail.h"
#include "../../imgui/imgui.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>
#include "../../engine/asset_system/asset_system.h"
#include "./widget_hierarchy.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"
#include "../../engine/scene/components/transform.h"

using namespace engine;
using namespace engine::asset_system;

namespace componentProperty
{
    static const float iconSize = 15.0f;

    inline bool Begin(
        const std::string& name, 
        asset_system::EngineAsset::IconInfo* icon, 
        std::shared_ptr<Component> component_instance)
    {
        const auto collapsed = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine();

        ImGui::Image(icon->getId(),ImVec2(iconSize,iconSize));
        ImGui::SameLine();

        return collapsed;
    }

    inline void End()
    {
        ImGui::Separator();
    }
}

WidgetDetail::WidgetDetail(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"细节面板";
	m_sceneManager = engine->getRuntimeModule<SceneManager>();
}

void WidgetDetail::onVisibleTick(size_t)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	if(const auto selectNode = EditorScene::get().getSelectNode().lock())
	{
		size_t id = selectNode->getId();
		if( id >= usageNode::start)
		{
			DrawTransform(selectNode);
		}
	}

	ImGui::End();
}

WidgetDetail::~WidgetDetail()
{

}

template<typename T, typename UIFunction>
static void drawComponent(const std::string& name, std::shared_ptr<engine::SceneNode> entity, UIFunction uiFunction,bool removable)
{
	const ImGuiTreeNodeFlags treeNodeFlags = 
		ImGuiTreeNodeFlags_DefaultOpen | 
		ImGuiTreeNodeFlags_Framed | 
		ImGuiTreeNodeFlags_SpanAvailWidth | 
		ImGuiTreeNodeFlags_AllowItemOverlap | 
		ImGuiTreeNodeFlags_FramePadding;

	if (entity->hasComponent<T>())
	{
		std::shared_ptr<T> component = entity->getComponent<T>();
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();
		bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
		ImGui::PopStyleVar();

		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
		{
			ImGui::OpenPopup("ComponentSettings");
		}

		bool removeComponent = false;
		if (ImGui::BeginPopup("ComponentSettings"))
		{
			if (ImGui::MenuItem(u8"移除组件"))
				removeComponent = removable;

			ImGui::EndPopup();
		}

		if (open)
		{
			uiFunction(component);
			ImGui::TreePop();
		}

		// TODO: 移除Component
	}
}

void WidgetDetail::DrawTransform(std::shared_ptr<SceneNode> n)
{
	if(std::shared_ptr<Transform> transform = n->getComponent<Transform>())
	{
		glm::vec3 translation = transform->getTranslation();
		glm::quat rotation = transform->getRotation();
		glm::vec3 scale = transform->getScale();
		glm::vec3 rotate_degree = glm::degrees(glm::eulerAngles(rotation));

		drawComponent<Transform>(u8"空间变换", n, [&](auto& component)
		{
			ImGui::Separator();
			Drawer::vector3(u8"坐标", translation,0.0f);
			ImGui::Separator();
			Drawer::vector3(u8"旋转", rotate_degree,0.0f);
			ImGui::Separator();
			Drawer::vector3(u8"缩放", scale, 1.0f);
			ImGui::Separator();
		},false);

		if(translation!=transform->getTranslation())
		{
			transform->setTranslation(translation);
		}

		if(scale!=transform->getScale())
		{
			transform->setScale(scale);
		}

		rotation = glm::quat(glm::radians(rotate_degree));
		if(rotation !=transform->getRotation())
		{
			transform->setRotation(rotation);
		}
	}

}
