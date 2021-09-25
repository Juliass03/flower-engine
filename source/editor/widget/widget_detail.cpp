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
#include "../../engine/renderer/material.h"
#include "../../engine/core/core.h"
#include "../../engine/renderer/mesh.h"
#include "../../engine/scene/components/staticmesh_renderer.h"

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
	auto& activeScene = m_sceneManager->getActiveScene();

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	if(const auto selectNode = EditorScene::get().getSelectNode().lock())
	{
		size_t id = selectNode->getId();
		if( id >= usageNodeIndex::start)
		{
			drawTransform(selectNode);
			
			drawStaticMesh(selectNode);

			ImGui::Spacing();
			ImGui::Separator();

			drawAddCommand(selectNode);
		}
	}

	ImGui::End();
}

WidgetDetail::~WidgetDetail()
{

}

template<typename T, typename UIFunction>
static void drawComponent(const std::string& name, std::shared_ptr<engine::SceneNode> entity, Scene& scene, UIFunction uiFunction,bool removable)
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

		if(removeComponent)
		{
			scene.removeComponent<T>(entity);
		}
	}
}

void WidgetDetail::drawTransform(std::shared_ptr<SceneNode> n)
{
	if(std::shared_ptr<Transform> transform = n->getComponent<Transform>())
	{
		auto& activeScene = m_sceneManager->getActiveScene();
		glm::vec3 translation = transform->getTranslation();
		glm::quat rotation = transform->getRotation();
		glm::vec3 scale = transform->getScale();
		glm::vec3 rotate_degree = glm::degrees(glm::eulerAngles(rotation));

		drawComponent<Transform>(u8"空间变换", n,activeScene, [&](auto& component)
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

template<typename Function>
void DrawButtonString(std::string idName,std::string& inout,Function callBack)
{
	ImGuiIO& io = ImGui::GetIO();
	auto boldFont = io.Fonts->Fonts[0];
	ImGui::PushID(idName.c_str());
	float size = ImGui::CalcTextSize(idName.c_str()).x;
	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, size + ImGui::GetColumnOffset());
	ImGui::Text(idName.c_str());
	ImGui::NextColumn();

	if(ImGui::Button(inout.c_str()))
	{
		callBack();
	}
	ImGui::Columns(1);
	ImGui::PopID();
}

void WidgetDetail::drawStaticMesh(std::shared_ptr<engine::SceneNode> node)
{
	auto& activeScene = m_sceneManager->getActiveScene();
	if(!node->hasComponent<StaticMeshComponent>())
		return;

	if(const auto component = node->getComponent<StaticMeshComponent>())
	{
		drawComponent<StaticMeshComponent>(u8"静态网格", node,activeScene, [&](auto& in_component)
		{
			ImGui::Separator();

			// Mesh Select
			{
				std::string idName = u8"网格";
				ImGuiIO& io = ImGui::GetIO();
				auto boldFont = io.Fonts->Fonts[0];
				ImGui::PushID(idName.c_str());
				float size = ImGui::CalcTextSize(idName.c_str()).x;
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, size + ImGui::GetColumnOffset());
				ImGui::Text(idName.c_str());
				ImGui::NextColumn();

				if(ImGui::Button(component->m_meshName.c_str()))
				{
					ImGui::OpenPopup("##StaticMeshComponentSelect");
				}

				if(ImGui::BeginPopup("##StaticMeshComponentSelect"))
				{
					const auto selectedNode = EditorScene::get().getSelectNode().lock();
					const auto onNode  = selectedNode->getId() != usageNodeIndex::empty;

					if (onNode)
					{
						loopPrimitiveType([&](std::string name){
							if (ImGui::MenuItem(name.c_str()))
							{
								component->m_meshName = name;
							}
						}); 
					}
					ImGui::EndPopup();
				}
				

				ImGui::Columns(1);
				ImGui::PopID();
			}
			
			ImGui::Separator();
		},true);
	}
}

void WidgetDetail::drawAddCommand(std::shared_ptr<engine::SceneNode> node)
{
	auto& activeScene = m_sceneManager->getActiveScene();

	ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f);
	if (ImGui::Button(u8"添加组件"))
	{
		ImGui::OpenPopup("##ComponentContextMenu_Add");
	}

    if (ImGui::BeginPopup("##ComponentContextMenu_Add"))
    {
		// Static Mesh
		if (ImGui::MenuItem(u8"静态网格") && !node->hasComponent(EComponentType::StaticMeshComponent))
		{
			activeScene.addComponent(std::make_shared<StaticMeshComponent>(),node);
			activeScene.setDirty(true);
			LOG_INFO("Node {0} has add static mesh component.",node->getName());
		}

        ImGui::EndPopup();
    }

}
