#pragma once
#include "widget.h"

namespace engine
{
	class SceneManager;
	class SceneNode;
	class Scene;
}

class WidgetDetail: public Widget
{
public:
	WidgetDetail(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetDetail();

private:
	void drawTransform(std::shared_ptr<engine::SceneNode>);
	void drawStaticMesh(std::shared_ptr<engine::SceneNode>);
	void drawAddCommand(std::shared_ptr<engine::SceneNode>);

private:
	engine::Ref<engine::SceneManager> m_sceneManager;
};