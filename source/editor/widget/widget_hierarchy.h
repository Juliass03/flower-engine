#pragma once
#include "widget.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"

class WidgetHierarchy : public Widget
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

	 char buf[32]; // ”√”⁄InputText
	 
	 bool m_expand_to_selection = false;
	 bool m_expanded_to_selection = false;
	 ImRect m_selected_entity_rect;
};