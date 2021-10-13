#version 460
#define FRAGMENT_SHADER

#include "../../glsl/common.glsl"
#include "../../glsl/frame_data.glsl"
#include "../../glsl/view_data.glsl"
#include "../../glsl/pass_depth.glsl"

void main() 
{
    outColor = vec4(1.0f,0.0f,0.0f,0.0f);
}