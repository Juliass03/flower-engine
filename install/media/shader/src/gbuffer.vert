#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_mesh.glsl"
#include "../glsl/common_framedata.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV0;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out vec3 outWorldNormal;
layout (location = 1) out vec2 outUV0;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outViewPos;
layout (location = 4) out vec4 outTangent;
layout (location = 5) out vec4 outPrevPosNoJitter;
layout (location = 6) out vec4 outCurPosNoJitter;
layout (location = 7) out flat uint outMaterialId;

void main()
{
	outUV0 = inUV0;
	
	mat4 curJitterMat = mat4(1.0f);
	curJitterMat[3][0] += frameData.jitterData.x;
	curJitterMat[3][1] += frameData.jitterData.y;

	uint objId = drawIndirectBuffer.indirectDraws[gl_DrawID].objectId;
	uint materialId = drawIndirectBuffer.indirectDraws[gl_DrawID].materialId;
	outMaterialId = materialId;

    mat4 modelMatrix  = perObjectBuffer.objects[objId].model;
	vec4 worldPos = modelMatrix * vec4(inPosition,1.0f);

	outWorldPos = vec3(worldPos);
	outViewPos = (frameData.camView * worldPos).xyz;
	outCurPosNoJitter =  frameData.camViewProj * worldPos;

	gl_Position = curJitterMat * outCurPosNoJitter;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
	outWorldNormal = normalMatrix * normalize(inNormal);
	outTangent =  vec4(normalMatrix * normalize(inTangent.xyz),inTangent.w);

	mat4 prevTransMatrix = perObjectBuffer.objects[objId].preModel;
	vec3 worldPrevPos = (prevTransMatrix * vec4(inPosition, 1.0f)).xyz;
	outPrevPosNoJitter = frameData.camViewProjLast * vec4(worldPrevPos, 1);
}