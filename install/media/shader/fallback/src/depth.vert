#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../../glsl/common.glsl"
#include "../../glsl/common_mesh.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV0;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out flat uint outInstanceIndex;
layout (location = 1) out vec2 outUV0;

void main()
{
	outUV0 = inUV0;

	mat4 modelMatrix  = perObjectBuffer.objects[gl_DrawID].model;
	mat4 mvp = frameData.camViewProj * modelMatrix;

	gl_Position = mvp * vec4(inPosition, 1.0f);

	outInstanceIndex = gl_DrawID;
}