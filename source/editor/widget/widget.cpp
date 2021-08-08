#include "widget.h"

using namespace engine;

uint32_t Widget::s_widgetCount = 0;

Widget::Widget(Ref<Engine> engine)
{
	m_engine = engine;
	m_renderer = engine->getRuntimeModule<Renderer>();

	s_widgetCount ++;
}

void Widget::tick()
{
	onTick();

	if(m_visible)
	{
		onVisibleTick();
	}
}

void Widget::release()
{
	onRelease();
}