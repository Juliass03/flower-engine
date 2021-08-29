#pragma once
#include "widget.h"

namespace engine
{
	class SceneManager;
	class SceneNode;
	class Scene;
}

enum class EComponent: uint32_t
{
	Transform,
};

class EditorComponent
{
public:
	std::weak_ptr<engine::SceneNode> inspectNode;

};

class WidgetDetail: public Widget
{
public:
	WidgetDetail(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetDetail();

	

private:
	void DrawTransform(std::shared_ptr<engine::SceneNode>);



private:
	engine::Ref<engine::SceneManager> m_sceneManager;
};