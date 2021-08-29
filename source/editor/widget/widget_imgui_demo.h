#pragma once
#include "widget.h"

class WidgetImguiDemo: public Widget
{
public:
	WidgetImguiDemo(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
};