#include "input.h"
#include "windowData.h"

namespace engine{

glm::vec2 gScrollOffset = { 0.0f,0.0f };
static bool bMouseInViewport = false;

float Input::getMouseX()
{
    return getMousePosition().x;
}

float Input::getMouseY()
{
    return getMousePosition().y;
}

glm::vec2 Input::getScrollOffset()
{
    return gScrollOffset;
}

float Input::getScrollOffsetX()
{
    return gScrollOffset.x;
}

float Input::getScrollOffsetY()
{
    return gScrollOffset.y;
}

bool Input::isMouseInViewport()
{
    return bMouseInViewport;
}

void Input::setMouseInViewport(bool bIn)
{
    bMouseInViewport = bIn;
}

void Input::setScrollOffset(float xOffset,float yOffset)
{
    gScrollOffset.x = xOffset;
    gScrollOffset.y = yOffset;
}

bool Input::isKeyPressed(const KeyCode key)
{
    auto state = glfwGetKey(g_windowData.window, static_cast<int32_t>(key));
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::isMouseButtonPressed(const MouseCode button)
{
    auto state = glfwGetMouseButton(g_windowData.window, static_cast<int32_t>(button));
    return state == GLFW_PRESS;
}

glm::vec2 Input::getMousePosition()
{
    double xpos, ypos;
    glfwGetCursorPos(g_windowData.window, &xpos, &ypos);

    return { (float)xpos, (float)ypos };
}

}