#pragma once
#include "widget.h"

class WidgetDownbar: public Widget
{
public:
	WidgetDownbar(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetDownbar();
};