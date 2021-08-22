#pragma once
#include <glm/glm.hpp>
#include "input_code.h"

namespace engine{
class Input
{
public:
    static bool isKeyPressed(const KeyCode key);
    static bool isMouseButtonPressed(MouseCode button);
    static glm::vec2 getMousePosition();
    static float getMouseX();
    static float getMouseY();
    static glm::vec2 getScrollOffset();
    static float getScrollOffsetX();
    static float getScrollOffsetY();

private:
    static void setScrollOffset(float xOffset,float yOffset);
    friend struct GLFWWindowData;
};
}