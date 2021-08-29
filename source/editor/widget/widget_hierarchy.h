#pragma once
#include "widget.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"
namespace engine
{
	class SceneManager;
	class SceneNode;
	class Scene;
}

struct SceneNodeWrapper
{
	std::weak_ptr<engine::SceneNode> node;
	size_t id;
};

class EditorScene
{
public:
	// ��һ��ѡ��Ľڵ�
	std::weak_ptr<engine::SceneNode> hoverNode;
	std::weak_ptr<engine::SceneNode> leftClickNode;

	std::weak_ptr<engine::SceneNode> rightClickNode; // �Ҽ���ť��Ҳ��ѡ�а�ť
	SceneNodeWrapper dragNode; // ��ק�Ľڵ�

	bool bRename = false;

	std::weak_ptr<engine::SceneNode> copiedNode;

	static EditorScene& get()
	{
		static EditorScene instance;
		return instance;
	}

	std::weak_ptr<engine::SceneNode> getSelectNode() { return rightClickNode; }
};

class WidgetHierarchy: public Widget
{
public:
	WidgetHierarchy(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetHierarchy();

private:
	void drawScene();
	void drawTreeNode(std::shared_ptr<engine::SceneNode>);

	void handleEvent();
	void popupMenu();

	void actionNodeDelete(const std::shared_ptr<engine::SceneNode>& entity);
	 std::shared_ptr<engine::SceneNode> actionNodeCreateEmpty();

	 void setSelectNode(std::weak_ptr<engine::SceneNode> node);

private:
	 engine::Ref<engine::SceneManager> m_sceneManager;
	 std::shared_ptr<engine::SceneNode> m_emptyNode;

	 char buf[32]; // ����InputText
	 
	 bool m_expand_to_selection = false;
	 bool m_expanded_to_selection = false;
	 ImRect m_selected_entity_rect;
};