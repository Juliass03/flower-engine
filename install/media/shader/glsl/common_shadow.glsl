#ifndef COMMON_SHADOW_GLSL
#define COMMON_SHADOW_GLSL

#include "common_poissondisk.glsl"

float shadowPcf(
    uint cascadeIndex,
    sampler2DArray shadowdepthTex,
    vec4 shadowCoord,
    vec2 texelSize,
    float dilation,
    bool bReverseZ)
{
	float shadowMaskColor = 0.0f;

    const uint pcfCount = 64;
    float count = 0.0f;

    float compareDepth = shadowCoord.z;
    compareDepth = bReverseZ ? compareDepth + 0.005f : compareDepth - 0.005f;

	for (int x = 0; x < pcfCount; x++)
	{
		vec2 offsetUv = texelSize * poisson_disk_64[x]  * dilation;

        float shadowMapDepth = texture(shadowdepthTex, vec3(shadowCoord.xy + offsetUv,cascadeIndex)).r;
		shadowMaskColor += shadowMapDepth > compareDepth ? 1.0f : 0.0f;

        count += 1.0f;
	}

	shadowMaskColor /= count;

	return shadowMaskColor;
}

#endif