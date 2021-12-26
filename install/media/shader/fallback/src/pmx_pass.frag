#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../../glsl/common.glsl"
#include "../../glsl/common_mesh.glsl"
#include "../../glsl/common_framedata.glsl"

layout (location = 0) in  vec3 inWorldNormal;
layout (location = 1) in  vec2 inUV0;
layout (location = 2) in  vec3 inWorldPos;

layout (location = 0) out vec4 outHdrColor; 
layout (location = 1) out vec2 outVelocity;

struct GPUPushConstants
{
    mat4 modelMatrix;
    uint baseColorTexId;
};	

layout(push_constant) uniform constants{   
   GPUPushConstants pushConstant;
};

void main() 
{
    vec4 baseColorTex = tex(pushConstant.baseColorTexId, inUV0);
    vec3 worldSpaceNormal = normalize(inWorldNormal);

    outHdrColor = vec4(baseColorTex.rgb, 1.0f);
    outVelocity = vec2(0.0f);
}