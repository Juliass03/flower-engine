#version 460
#define VERTEX_SHADER

#include "../glsl/common.glsl"
#include "../glsl/frame_data.glsl"
#include "../glsl/view_data.glsl"
#include "../glsl/pass_gbuffer.glsl"

void main()
{
    mat4 modelMatrix  = objectBuffer.objects[gl_BaseInstance].model;
	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
	mat4 transformMatrix = viewData.camViewProj * modelMatrix;

	gl_Position = transformMatrix * vec4(inPosition, 1.0f);

	vec4 worldPos = modelMatrix * vec4(inPosition,1.0f);
	outWorldPos = vec3(worldPos);
	outWorldNormal = normalMatrix * normalize(inNormal);
	outTangent =  vec4(normalMatrix * normalize(inTangent.xyz),inTangent.w);
	outUV0 = inUV0;

	outViewPos = (viewData.camView * worldPos).xyz;
}