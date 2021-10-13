#ifndef PASS_TONEMAPPER_GLSL
#define PASS_TONEMAPPER_GLSL

layout (set = 2, binding = 0) uniform sampler2D scenecolorTex;

#ifdef VERTEX_SHADER
    layout (location = 0) out vec2 outUV;
#endif

#ifdef FRAGMENT_SHADER
    layout (location = 0) in vec2 inUV0;
    layout (location = 0) out vec4 outColor;
#endif

#endif