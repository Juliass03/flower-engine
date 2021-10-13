#version 460
#define FRAGMENT_SHADER

#include "../../glsl/common.glsl"
#include "../../glsl/frame_data.glsl"
#include "../../glsl/view_data.glsl"
#include "../../glsl/pass_lighting.glsl"

void main()
{
    outHdrSceneColor.rgb = texture(inGbufferBaseColorMetal,inUV0).xyz;
    outHdrSceneColor.a = 1.0f;
}