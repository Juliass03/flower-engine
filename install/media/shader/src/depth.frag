#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_mesh.glsl"

layout (location = 0) in  vec2 inUV0;
layout (location = 1) in  flat uint inBaseColorTexId;

layout (location = 0) out vec4 outColor;

void main() 
{
    vec4 baseColorTex = tex(inBaseColorTexId,inUV0);

    if(baseColorTex.a < 0.5f)
    {
        discard;
    }
    outColor = vec4(0.0f,0.0f,0.0f,0.0f);
}