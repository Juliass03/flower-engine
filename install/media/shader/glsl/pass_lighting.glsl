#ifndef PASS_LIGHTING_GLSL
#define PASS_LIGHTING_GLSL

layout (set = 2, binding = 0) uniform sampler2D inGbufferBaseColorMetal;
layout (set = 2, binding = 1) uniform sampler2D inGbufferNormalRoughness;
layout (set = 2, binding = 2) uniform sampler2D inGbufferEmissiveAo;
layout (set = 2, binding = 3) uniform sampler2D inDepth;

#ifdef VERTEX_SHADER
    layout (location = 0) out vec2 outUV;
#endif

#ifdef FRAGMENT_SHADER
    layout (location = 0) in vec2 inUV0;
    layout (location = 0) out vec4 outHdrSceneColor;
#endif

#endif