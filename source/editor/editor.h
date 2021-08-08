#pragma once
#include <vector>

class Widget;
class Editor
{
public:
	Editor() = default;

	void init();
	void loop();
	void release();

private:
	std::vector<Widget*> m_widgets;
};