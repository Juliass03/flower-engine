#ifndef COMMON_SHADOW_GLSL
#define COMMON_SHADOW_GLSL

#include "common_poissondisk.glsl"

float shadowPcf(
    uint cascadeIndex,
    sampler2DArray shadowdepthTex,
    vec4 shadowCoord,
    vec2 texelSize,
    float dilation,
    bool bReverseZ,
    float NoLSafe)
{
	float shadowMaskColor = 0.0f;
    const uint pcfCount = 25;
    float count = 0.0f;
    float compareDepth = shadowCoord.z;

	for (int x = 0; x < pcfCount; x++)
	{
		vec2 offsetUv = texelSize * poisson_disk_25[x] * dilation;
        float shadowMapDepth = texture(shadowdepthTex, vec3(shadowCoord.xy + offsetUv,cascadeIndex)).r;

        shadowMaskColor += shadowMapDepth < compareDepth  ? 1.0f : 0.0f;

        /*
        if(bReverseZ)
        {
        }
        else
        {
            shadowMaskColor += shadowMapDepth > (compareDepth - 0.005f) ? 1.0f : 0.0f;
        }
        */

        count += 1.0f;
	}

	shadowMaskColor /= count;
	return shadowMaskColor;
}

float random(vec3 seed, int i)
{
    vec4 seed4 = vec4(seed,i);
    float dotProduct = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dotProduct) * 43758.5453);
}

// pcss 查找遮挡物
vec2 pcssSearchBlocker(
    uint cascadeIndex,
    vec4 shadowCoord,
    sampler2DArray shadowdepthTex,
    vec2 texelSize,
    float dilation,
    bool bReverseZ) 
{
	float blockerDepth = 0.0;
	float count = 0.0;
    uint pcssCount = 25;

	for (int x = 0; x < pcssCount; x++)
	{
		int index = int( float(pcssCount) * random(shadowCoord.xyy,x) ) % int(pcssCount);
		vec2 sample_uv = dilation * poisson_disk_25[index] * texelSize + shadowCoord.st;
		float dist = texture(shadowdepthTex, vec3(sample_uv,cascadeIndex)).r;

		if(bReverseZ)
        {
            if(dist > shadowCoord.z)
            {
                blockerDepth += dist;
                count += 1.0f;
            }
        }
        else if(dist < shadowCoord.z)
        {
            blockerDepth += dist;
            count += 1.0f;
        }
	}

	if(count > 0.5f)
	{
		return vec2(blockerDepth / count, 1.0f);
	}
	else
	{
		return vec2(1.0f,0.0f);
	}
}

// pcss软阴影
float shadowPcss(
    float minPcfDialtion,
    uint cascadeIndex,
    vec4 shadowCoord,
    sampler2DArray shadowdepthTex, 
    float pcfDilation,
    float pcssDilation,
    bool bReverseZ,
    float NoLSafe)
{
	ivec2 texDim = textureSize(shadowdepthTex, 0).xy;
	float dx = 1.0 / float(texDim.x);
	float dy = 1.0 / float(texDim.y);
    vec2 texelSize = vec2(dx,dy);
    
	vec2 ret = pcssSearchBlocker(
        cascadeIndex,
        shadowCoord,
        shadowdepthTex,
        texelSize, 
        pcssDilation,
        bReverseZ
    );

	float avgBlockerDepth = ret.x;
	if(ret.y < 0.5f)
	{
		// 提前退出节省非阴影区域的pcf消耗。
		return 1.0f; 
	}
    avgBlockerDepth = max(avgBlockerDepth,0.1f);
	float penumbraSize = abs(avgBlockerDepth - shadowCoord.z) / avgBlockerDepth * pcfDilation;

	return shadowPcf(
        cascadeIndex,
        shadowdepthTex,
        shadowCoord,
        texelSize, 
        max(minPcfDialtion,penumbraSize),
        bReverseZ,
        NoLSafe
    );
}

#endif