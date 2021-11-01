#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_mesh.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV0;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out vec2 outUV0;
layout (location = 1) out flat uint outBaseColorTexId;

struct DepthDrawPushConstants
{
	uint cascadeIndex;
};	

layout(push_constant) uniform constants
{   
   DepthDrawPushConstants cascadeInfos;
};

void main()
{
	outUV0 = inUV0;

	uint objId = drawIndirectBuffer.indirectDraws[gl_DrawID].objectId;
	uint materialId = drawIndirectBuffer.indirectDraws[gl_DrawID].materialId;

	mat4 modelMatrix  = perObjectBuffer.objects[objId].model;
	mat4 mvp = frameData.cascadeViewProjMatrix[cascadeInfos.cascadeIndex] * modelMatrix;

	gl_Position = mvp * vec4(inPosition, 1.0f);

	PerObjectMaterialData matData = perObjectMaterial.materials[materialId];
	outBaseColorTexId = matData.baseColorTexId;
}