#version 460
#define FRAGMENT_SHADER

#include "../../glsl/common.glsl"
#include "../../glsl/frame_data.glsl"
#include "../../glsl/view_data.glsl"
#include "../../glsl/pass_tonemapper.glsl"

void main()
{
    vec3 scenecolor = texture(scenecolorTex, inUV0).xyz;
    outColor = vec4(scenecolor,1.0f);
}