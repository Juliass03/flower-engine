#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_framedata.glsl"

layout (set = 1, binding = 0) uniform sampler2D scenecolorTex;

layout (location = 0) in  vec2 inUV0;
layout (location = 0) out vec4 outColor;

void main()
{
    vec3 scenecolor = texture(scenecolorTex, inUV0).xyz;
    outColor = vec4(scenecolor,1.0f);
}