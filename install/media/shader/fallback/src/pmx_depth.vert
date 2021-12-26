#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../../glsl/common.glsl"
#include "../../glsl/common_mesh.glsl"
#include "../../glsl/common_framedata.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;

layout (location = 0) out vec2 outUV0;

layout(set = 5, binding = 0) readonly buffer CascadeInfoBuffer
{
	CascadeInfo cascadeInfosbuffer[];
};

struct GPUPushConstants
{
    mat4 modelMatrix;
    uint cascadeIndex;
};	

layout(push_constant) uniform constants{   
   GPUPushConstants pushConstant;
};

void main()
{
	outUV0 = inUV0;

    mat4 modelMatrix = pushConstant.modelMatrix;
    mat4 mvp = cascadeInfosbuffer[pushConstant.cascadeIndex].cascadeViewProjMatrix * modelMatrix;

	gl_Position = mvp * vec4(inPosition, 1.0f);
}