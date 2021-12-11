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
#include "../../engine/scene/components/directionalLight.h"
#include "widget_asset.h"
#include "widget_assetInspector.h"

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
	m_title = u8"Detail";
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

			drawDirectionalLight(selectNode);

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
			if (ImGui::MenuItem(u8"Remove Component"))
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

		drawComponent<Transform>(u8"Transform", n,activeScene, [&](auto& component)
		{
			ImGui::Separator();
			Drawer::vector3(u8"Location", translation,0.0f);
			ImGui::Separator();
			Drawer::vector3(u8"Rotation", rotate_degree,0.0f);
			ImGui::Separator();
			Drawer::vector3(u8"Scale", scale, 1.0f);
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
		drawComponent<StaticMeshComponent>(u8"StaticMesh", node,activeScene, [&](auto& in_component)
		{
			ImGui::Separator();

			// Mesh Select
			{
				std::string idName = u8"Mesh";
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

				std::string oldPrimitveName = component->m_meshName;
				std::string oldCutomName = component->m_customMeshName;
				bool oldUseCustomMesh = component->m_customMesh;

				if(ImGui::BeginPopup("##StaticMeshComponentSelect"))
				{
					const auto selectedNode = EditorScene::get().getSelectNode().lock();
					const auto onNode  = selectedNode->getId() != usageNodeIndex::empty;

					if (onNode)
					{
						loopPrimitiveType([&](std::string name,EPrimitiveMesh type){
							if (ImGui::MenuItem(name.c_str()))
							{
								component->m_meshName = name;
								if(type==EPrimitiveMesh::Custom)
								{
									component->m_customMesh = true;
								}
								else
								{
									component->m_customMesh = false;
								}
							}
						}); 


					}
					ImGui::EndPopup();
				}

				if(component->m_customMesh)
				{
					const auto& meshes = MeshLibrary::get()->getStaticMeshList();
					static int lastTimeSize = -1;

					// TODO: This is slow and need optimization.
					int loopIndex = 0;
					std::vector<const char*> cacheName{};
					std::string boxName = toString(EPrimitiveMesh::Box);
					cacheName.push_back(boxName.c_str()); // 第0个永远为Box作为回滚
					for(const auto& elem : meshes)
					{
						if(component->m_customMeshName == elem && lastTimeSize != int(meshes.size() + 1)) // 防止队列刷新后出现错误的选择
						{
							component->m_selectMesh = loopIndex + 1; // 注意处理插入的第0个box
						}
						cacheName.push_back(elem.c_str());
						loopIndex++;
					}

					if(cacheName.size()>0)
					{
						if(component->m_selectMesh < cacheName.size())
						{
							ImGui::Combo(u8"Select Mesh",&component->m_selectMesh,cacheName.data(),int(cacheName.size()),-1);

							if(lastTimeSize != int(cacheName.size()) && component->m_customMeshName != cacheName[component->m_selectMesh])
							{
								component->m_customMeshName = boxName; // 删除选择的物体后使用引擎Box作为回滚
								component->m_selectMesh = 0;
							}
							else
							{
								component->m_customMeshName = cacheName[component->m_selectMesh];
							}
						}
					}
					lastTimeSize = int(cacheName.size());
				}

				ImGui::Columns(1);
				ImGui::PopID();

				// 开始监视每个submesh的材质
				if(!(component->m_customMesh==oldUseCustomMesh &&
					component->m_customMeshName==oldCutomName  &&
					component->m_meshName == oldPrimitveName)) 
				{
					component->reflectMaterials(); // 网格发生变化则反射一次材质数据
				}

				for(auto& material : component->m_materials)
				{
					if(ImGui::Button(material.c_str()))
					{
						EditorAsset::get().inspectorRequireSkip = true;
						EditorAsset::get().inspectorClickAssetPath = material;
					}
				}
			}
			
			ImGui::Separator();
		},true);
	}
}

void WidgetDetail::drawDirectionalLight(std::shared_ptr<engine::SceneNode> node)
{
	auto& activeScene = m_sceneManager->getActiveScene();
	if(!node->hasComponent<DirectionalLight>())
		return;

	if(const auto component = node->getComponent<DirectionalLight>())
	{
		drawComponent<DirectionalLight>(u8"Sun Light", node,activeScene, [&](auto& in_component)
		{
			ImGui::Separator();

			{
				glm::vec4 oldColor = in_component->getColor();
				static ImVec4 color;
				color = ImVec4(oldColor.x, oldColor.y, oldColor.z, 1.0f);
				ImGui::ColorPicker4(u8"Color",(float*)&color,ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_NoBorder);
				glm::vec4 newColor = glm::vec4(color.x,color.y,color.z,1.0f);
				
				if(newColor!=oldColor)
				{
					in_component->setColor(newColor);
				}
			}

			ImGui::Separator();
		},true);
	}
}

void WidgetDetail::drawAddCommand(std::shared_ptr<engine::SceneNode> node)
{
	auto& activeScene = m_sceneManager->getActiveScene();

	ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f);
	if (ImGui::Button(u8"Add Component"))
	{
		ImGui::OpenPopup("##ComponentContextMenu_Add");
	}

    if (ImGui::BeginPopup("##ComponentContextMenu_Add"))
    {
		// Static Mesh
		if (ImGui::MenuItem(u8"StaticMesh") && !node->hasComponent(EComponentType::StaticMeshComponent))
		{
			activeScene.addComponent(std::make_shared<StaticMeshComponent>(),node);
			activeScene.setDirty(true);
			LOG_INFO("Node {0} has add static mesh component.",node->getName());
		}

		// Directional Light
		if (ImGui::MenuItem(u8"Sun Light") && !node->hasComponent(EComponentType::DirectionalLight))
		{
			activeScene.addComponent(std::make_shared<DirectionalLight>(),node);
			activeScene.setDirty(true);
			LOG_INFO("Node {0} has add directional light component.",node->getName());
		}

        ImGui::EndPopup();
    }

}
