#pragma once
#include "core.h"
#include <glfw/glfw3.h>
#include "input.h"

namespace engine{

struct GLFWWindowData
{
	GLFWwindow* window;
	bool bFoucus = true;

	void CallbackOnResize(int width, int height) 
	{
		LOG_INFO("Window resize: ({0},{1}).",width,height);
	}

	void CallbackOnMouseMove(double xpos, double ypos) {  }
	void CallbackOnMouseButton(int button, int action, int mods) { }

	void CallbackOnScroll(double xoffset, double yoffset)
	{ 
		Input::setScrollOffset((float)xoffset,(float)yoffset);
	}
};

extern GLFWWindowData g_windowData; 
}