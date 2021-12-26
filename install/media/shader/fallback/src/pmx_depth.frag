#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../../glsl/common.glsl"
#include "../../glsl/common_mesh.glsl"
#include "../../glsl/common_framedata.glsl"

layout (location = 0) in  vec2 inUV0;
layout (location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(0.0f,0.0f,0.0f,0.0f);
}