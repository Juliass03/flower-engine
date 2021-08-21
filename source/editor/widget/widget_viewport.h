#pragma once
#include "widget.h"

class WidgetViewport: public Widget
{
public:
	WidgetViewport(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick() override;
	~WidgetViewport();

private:
	float m_sceneViewWidth = 0.0f;
	float m_sceneViewHeight = 0.0f;
	glm::vec2 m_sceneViewOffset { 0.0f };
};