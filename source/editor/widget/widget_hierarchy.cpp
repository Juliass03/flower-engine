#include "widget_hierarchy.h"
#include "../../imgui/imgui.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>
#include "../../engine/asset_system/asset_system.h"
#include "../../engine/scene/scene.h"
#include "../../engine/scene/scene_node.h"

using namespace engine;
using namespace engine::asset_system;

WidgetHierarchy::WidgetHierarchy(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = u8"场景大纲";
	m_sceneManager = engine->getRuntimeModule<SceneManager>();

	m_emptyNode = std::make_shared<SceneNode>(usageNode::empty,"empty");
	EditorScene::get().rightClickNode = m_emptyNode;
}

WidgetHierarchy::~WidgetHierarchy()
{
	m_sceneManager = nullptr;
}

void WidgetHierarchy::onVisibleTick(size_t)
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(m_title.c_str(), &m_visible))
	{
		ImGui::End();
		return;
	}

	drawScene();

	if (ImGui::IsMouseReleased(0) && EditorScene::get().leftClickNode.lock() && EditorScene::get().leftClickNode.lock()->getId() != usageNode::empty)
	{
		if (EditorScene::get().hoverNode.lock() && EditorScene::get().hoverNode.lock()->getId() == EditorScene::get().leftClickNode.lock()->getId())
		{
			setSelectNode(EditorScene::get().leftClickNode);
		}

		EditorScene::get().leftClickNode.reset();
	}

	m_expand_to_selection = true;

	ImGui::End();
}

void WidgetHierarchy::drawScene()
{
	EditorScene::get().hoverNode.reset();
	auto& activeScene = m_sceneManager->getActiveScene();
	auto rootNode = activeScene.getRootNode();
	
	auto& children = rootNode-> getChildren();
	for(auto& child : children)
	{
		CHECK(child);
		drawTreeNode(child);
	}

	if (m_expand_to_selection && !m_expanded_to_selection)
	{
		ImGui::ScrollToBringRectIntoView(ImGui::GetCurrentWindow(), m_selected_entity_rect);
		m_expand_to_selection = false;
	}

	handleEvent();
	popupMenu();
}

std::string DragDropId = "Hierarchy_SceneNode";

void WidgetHierarchy::drawTreeNode(std::shared_ptr<engine::SceneNode> node)
{
	const bool bTreeNode = node->getChildren().size() > 0;

	if(bTreeNode)
	{
		ImGui::Separator();
	}

	m_expanded_to_selection = false;
	bool is_selected_entity = false;

	
	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanFullWidth;
	node_flags |= bTreeNode ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf; 

	if(const auto selectedNode = EditorScene::get().getSelectNode().lock())
	{
		node_flags |= selectedNode->getId() == node->getId() ? ImGuiTreeNodeFlags_Selected : node_flags;

		if (m_expand_to_selection)
		{
			if (selectedNode->getParent() && selectedNode->getParent()->getId() == node->getId())
			{
				ImGui::SetNextItemOpen(true);
				m_expanded_to_selection = true;
			}
		}
	}

	bool bIsEditingName = false;
	bool bNodeOpen;
	if(EditorScene::get().bRename && EditorScene::get().getSelectNode().lock() && EditorScene::get().getSelectNode().lock()->getId() == node->getId())
	{
		bNodeOpen = true;
		bIsEditingName = true;
		strcpy(buf, node->getName().c_str());
		if (ImGui::InputText(" ",buf,IM_ARRAYSIZE(buf),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll))
		{
			node->setName(buf);
			EditorScene::get().bRename = false;
		}

		if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			EditorScene::get().bRename = false;
		}
	}
	else
	{
		bNodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->getId())), node_flags, node->getName().c_str());
	}

	if ((node_flags & ImGuiTreeNodeFlags_Selected) && m_expand_to_selection)
	{
		m_selected_entity_rect = ImGui::GetContext()->LastItemData.Rect;
	}

	// 检测是否当前项被选择
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
	{
		EditorScene::get().hoverNode = node;
	}
	
	if (ImGui::BeginDragDropSource())
	{
		
		EditorScene::get().dragNode.node = node;
		EditorScene::get().dragNode.id = node->getId();

		ImGui::SetDragDropPayload(DragDropId.c_str(), &EditorScene::get().dragNode.id, sizeof(size_t));

		ImGui::Text(node->getName().c_str());
		ImGui::EndDragDropSource();
	}

	if(EditorScene::get().hoverNode.lock())
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DragDropId.c_str()))
			{
				if(const auto payloadNode = EditorScene::get().dragNode.node.lock())
				{
					auto oldP = payloadNode->getParent();
					std::shared_ptr<SceneNode> node = m_sceneManager->getActiveScene().getRootNode();
					node = EditorScene::get().hoverNode.lock();
					if(node->getId() != oldP->getId())
					{
						if(!payloadNode->isSon(node))
						{
							payloadNode->setParent(node);
							node->addChild(payloadNode);
							oldP->removeChild(payloadNode);
						}

						EditorScene::get().dragNode.node.reset();
						EditorScene::get().dragNode.id = -2;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	if (bNodeOpen)
	{
		if (bTreeNode)
		{
			for (const auto& child : node->getChildren())
			{
				drawTreeNode(child);
			}
		}

		if(!bIsEditingName)
			ImGui::TreePop();
	}
}

void WidgetHierarchy::handleEvent()
{
	const auto bWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	if(!bWindowHovered)
	{
		return; 
	}
		
	const auto bLeftClick = ImGui::IsMouseClicked(0);
	const auto bRightClick = ImGui::IsMouseClicked(1);
	const auto bDoubleClick = ImGui::IsMouseDoubleClicked(0);

	// 检测点击项
	if (bLeftClick && EditorScene::get().hoverNode.lock())
	{
		EditorScene::get().leftClickNode = EditorScene::get().hoverNode;
	}

	// 右键
	if (bRightClick || bDoubleClick)
	{
		// 对准了词条右键
		if (EditorScene::get().hoverNode.lock())
		{            
			setSelectNode(EditorScene::get().hoverNode);
		}
	}

	if(bDoubleClick && EditorScene::get().getSelectNode().lock() && EditorScene::get().getSelectNode().lock()->getId() != usageNode::empty)
	{
		EditorScene::get().bRename = true;
	}

	if(bRightClick)
	{
		// 右键菜单
		ImGui::OpenPopup("##HierarchyContextMenu");
	}

	// 对准空白处点击取消先前的选中
	if ((bRightClick || bLeftClick) && !EditorScene::get().hoverNode.lock())
	{
		setSelectNode(m_emptyNode);
	}

	if(!EditorScene::get().hoverNode.lock())
	{
		if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(),ImGui::GetCurrentWindow()->ID))
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DragDropId.c_str()))
			{
				if(const auto payloadNode = EditorScene::get().dragNode.node.lock())
				{
					auto oldP = payloadNode->getParent();
					std::shared_ptr<SceneNode> node = m_sceneManager->getActiveScene().getRootNode();
					if(node->getId() != oldP->getId())
					{
						if(!payloadNode->isSon(node))
						{
							payloadNode->setParent(node);
							node->addChild(payloadNode);
							oldP->removeChild(payloadNode);
						}

						EditorScene::get().dragNode.node.reset();
						EditorScene::get().dragNode.id = -2;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
}

void WidgetHierarchy::popupMenu()
{
	auto ContextMenu = [&]()
	{
		if (!ImGui::BeginPopup("##HierarchyContextMenu"))
			return;

		const auto selectedNode = EditorScene::get().getSelectNode().lock();
		const auto onNode  = selectedNode->getId() != usageNode::empty;

		if (onNode && ImGui::MenuItem(u8"复制"))
		{
			EditorScene::get().copiedNode = selectedNode;
		}

		if (EditorScene::get().copiedNode.lock() && EditorScene::get().copiedNode.lock()->getId() != usageNode::empty && ImGui::MenuItem(u8"黏贴"))
		{
			// TODO: 克隆函数
		}

		if (onNode && ImGui::MenuItem(u8"重命名"))
		{
			EditorScene::get().bRename = true;
		}

		if (onNode && ImGui::MenuItem(u8"删除"))
		{
			actionNodeDelete(selectedNode);
		}

		ImGui::Separator();

		if (ImGui::MenuItem(u8"创建空物体"))
		{
			actionNodeCreateEmpty();
		}

		ImGui::EndPopup();
	};


	ContextMenu();
}

void WidgetHierarchy::actionNodeDelete(const std::shared_ptr<engine::SceneNode>& node)
{
	node->selfDelete();
}

std::shared_ptr<engine::SceneNode> WidgetHierarchy::actionNodeCreateEmpty()
{

	// TODO: 不重复的名字
	std::string name = "EmptyNode_GUID:" + std::to_string( m_sceneManager->getActiveScene().getLastGUID() + 1);
	auto newNode = m_sceneManager->getActiveScene().createNode(name.c_str());

	if(EditorScene::get().getSelectNode().lock() && EditorScene::get().getSelectNode().lock()->getId()!=usageNode::empty)
	{
		newNode->setParent(EditorScene::get().getSelectNode().lock());
		EditorScene::get().getSelectNode().lock()->addChild(newNode);
	}
	else
	{
		newNode->setParent(m_sceneManager->getActiveScene().getRootNode());

		m_sceneManager->getActiveScene().getRootNode()->addChild(newNode);
	}

	return newNode;
}

void WidgetHierarchy::setSelectNode(std::weak_ptr<engine::SceneNode> node)
{
	m_expand_to_selection = true;
	EditorScene::get().rightClickNode = node;
}
