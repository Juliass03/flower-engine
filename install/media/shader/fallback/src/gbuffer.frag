#version 460
#define FRAGMENT_SHADER

#include "../../glsl/common.glsl"
#include "../../glsl/frame_data.glsl"
#include "../../glsl/view_data.glsl"
#include "../../glsl/pass_gbuffer.glsl"

void main() 
{
    outGbufferBaseColorMetal = vec4(0.0f,1.0f,1.0f,0.0f);
    outGbufferNormalRoughness = vec4(normalize(inWorldNormal),1.0f);
    outGbufferEmissiveAo = vec4(0.0f,0.0f,0.0f,1.0f);
}