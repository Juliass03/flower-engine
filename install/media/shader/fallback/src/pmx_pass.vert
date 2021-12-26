#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../../glsl/common.glsl"
#include "../../glsl/common_mesh.glsl"
#include "../../glsl/common_framedata.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;

layout (location = 0) out vec3 outWorldNormal;
layout (location = 1) out vec2 outUV0;
layout (location = 2) out vec3 outWorldPos;

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
	outUV0 = inUV0;
	
	mat4 curJitterMat = mat4(1.0f);
	curJitterMat[3][0] += frameData.jitterData.x;
	curJitterMat[3][1] += frameData.jitterData.y;

    mat4 modelMatrix = pushConstant.modelMatrix;
    vec4 worldPos = modelMatrix * vec4(inPosition,1.0f);

	outWorldPos = vec3(worldPos);

	vec4 curPosNoJitter =  frameData.camViewProj * worldPos;
	gl_Position = curJitterMat * curPosNoJitter;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
	outWorldNormal = normalMatrix * normalize(inNormal);
}