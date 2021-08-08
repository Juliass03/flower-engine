#pragma once
#include "../../engine/engine.h"
#include "../../engine/renderer/renderer.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui.h"

class Widget
{
protected:
	engine::Ref<engine::Engine> m_engine;
	engine::Ref<engine::Renderer> m_renderer;
	static uint32_t s_widgetCount;
	// Common widget info.
	bool m_visible = true;
	std::string m_title = "Widget";

public:
	std::string getTile() const
	{
		return m_title + std::to_string(s_widgetCount);
	}

	engine::Ref<engine::Renderer> getRenderer()
	{
		return m_renderer;
	}

public:
	virtual ~Widget() {  }
	Widget() = delete;
	Widget(engine::Ref<engine::Engine> engine);

	void tick();
	void release();

protected:
	// Tick functions
	virtual void onShow() {  }
	virtual void onHide() {  }
	virtual void onTick() {  }
	virtual void onVisibleTick() {  }
	virtual void onRelease() {  }

}; 