#pragma once
#include "widget.h"

class WidgetConsole: public Widget
{
public:
	WidgetConsole(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~WidgetConsole();

private:
	struct WidgetConsoleApp* app;
};