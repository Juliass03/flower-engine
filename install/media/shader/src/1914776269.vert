#version 460

// Shader Pass: GBuffer

#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_mesh.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV0;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inTangent;

layout (location = 0) out vec3 outWorldNormal;
layout (location = 1) out vec2 outUV0;
layout (location = 2) out vec3 outWorldPos;
layout (location = 3) out vec3 outViewPos;
layout (location = 4) out vec4 outTangent;
layout (location = 5) out flat uint outInstanceIndex;

void main()
{
	outUV0 = inUV0;

    mat4 modelMatrix  = perObjectBuffer.objects[gl_DrawID].model;
	vec4 worldPos = modelMatrix * vec4(inPosition,1.0f);

	outWorldPos = vec3(worldPos);
	outViewPos = (frameData.camView * worldPos).xyz;

	mat4 mvp = frameData.camViewProj * modelMatrix;
	gl_Position = mvp * vec4(inPosition, 1.0f);

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
	outWorldNormal = normalMatrix * normalize(inNormal);
	outTangent =  vec4(normalMatrix * normalize(inTangent.xyz),inTangent.w);

	outInstanceIndex = gl_DrawID;
}
