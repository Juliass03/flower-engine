#pragma once
#include "core.h"
#include "input_code.h"

namespace engine{
namespace Input
{
    extern bool isKeyPressed(const KeyCode key);
    extern bool isMouseButtonPressed(MouseCode button);
    extern  glm::vec2 getMousePosition();
    extern float getMouseX();
    extern float getMouseY();
    extern glm::vec2 getScrollOffset();
    extern float getScrollOffsetX();
    extern float getScrollOffsetY();
    extern bool isMouseInViewport();
    extern void setMouseInViewport(bool);
    extern void setScrollOffset(float xOffset,float yOffset);
};
}