#ifndef COMMON_SHADOW_GLSL
#define COMMON_SHADOW_GLSL

#include "common_poissondisk.glsl"

float shadowPcf(
    uint cascadeIndex,
    sampler2DArrayShadow shadowdepthTex,
    vec4 shadowCoord,
    vec2 texelSize,
    float dilation)
{
	float shadowMaskColor = 0.0f;

    const uint pcfCount = 64;
    float count = 0.0f;

	for (int x = 0; x < pcfCount; x++)
	{
		vec2 offsetUv = texelSize * poisson_disk_64[x]  * dilation;
		shadowMaskColor += texture(
            shadowdepthTex, 
            vec4(
                shadowCoord.xy + offsetUv,
                cascadeIndex,
                shadowCoord.z + 0.005f
            )
        ).r;

        count += 1.0f;
	}

	shadowMaskColor /= count;
	return shadowMaskColor;
}

#endif