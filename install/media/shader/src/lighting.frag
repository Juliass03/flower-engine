#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_shadow.glsl"

layout (set = 1, binding = 0) uniform sampler2D inGbufferBaseColorMetal;
layout (set = 1, binding = 1) uniform sampler2D inGbufferNormalRoughness;
layout (set = 1, binding = 2) uniform sampler2D inGbufferEmissiveAo;
layout (set = 1, binding = 3) uniform sampler2D inDepth;

layout (set = 1, binding = 4) uniform sampler2DArrayShadow inShadowDepthBilinearTexture;

struct LightingPushConstants
{
	float pcfDilation;
};  

layout(push_constant) uniform constants{   
   LightingPushConstants pushConstants;
};

layout (location = 0) in  vec2 inUV0;
layout (location = 0) out vec4 outHdrSceneColor;

struct GbufferData
{
    vec3 baseColor;
    vec3 emissiveColor;
    vec3 worldNormal;

    vec3 worldPos;

    float metal;
    float roughness;
    float ao;

    float deviceDepth;
};

GbufferData loadGbufferData()
{
    GbufferData gData;

    vec4 packGbufferBaseColorMetal  = texture(inGbufferBaseColorMetal,inUV0);
    gData.baseColor = packGbufferBaseColorMetal.xyz;
    gData.metal     = packGbufferBaseColorMetal.w;

    vec4 packGbufferNormalRoughness = texture(inGbufferNormalRoughness,inUV0);
    gData.worldNormal = unpackGbufferNormal(normalize(packGbufferNormalRoughness.xyz));
    gData.roughness = packGbufferNormalRoughness.w;

    vec4 packGbufferEmissiveAo = texture(inGbufferEmissiveAo,inUV0);
    gData.emissiveColor = packGbufferEmissiveAo.xyz;
    gData.ao = packGbufferEmissiveAo.w;

    float sampleDeviceDepth = texture(inDepth,inUV0).r;
    gData.deviceDepth = sampleDeviceDepth;
    gData.worldPos = getWorldPosition(sampleDeviceDepth,inUV0);

    return gData;
}

float evaluateDirectShadow(vec3 fragWorldPos,vec3 normal,float safeNoL)
{
    ivec2 texDim = textureSize(inShadowDepthBilinearTexture,0).xy;
	vec2 texelSize = 1.0f / vec2(texDim);

    float shadowFactor = 1.0f;
    const uint cascadeCount = 4;

    for(uint cascadeIndex = 0; cascadeIndex < cascadeCount; cascadeIndex ++)
	{
        vec4 shadowClipPos = frameData.cascadeViewProjMatrix[cascadeIndex] * vec4(fragWorldPos, 1.0);

        vec4 shadowCoord = shadowClipPos / shadowClipPos.w;
		shadowCoord.st = shadowCoord.st * 0.5f + 0.5f;

        if( shadowCoord.x > 0.01f && shadowCoord.y > 0.01f && 
			shadowCoord.x < 0.99f && shadowCoord.y < 0.99f &&
			shadowCoord.z > 0.0f && shadowCoord.z < 1.0f)
		{
            shadowFactor = shadowPcf(
                cascadeIndex,
                inShadowDepthBilinearTexture,
                shadowCoord,
                texelSize,
                2.50f
            );

            break;
        }
    }
    return shadowFactor;
}

void main()
{
    GbufferData gData = loadGbufferData();

    vec3 sunDir = normalize(-frameData.sunLightDir.xyz);
    vec3 sunColor = frameData.sunLightColor.xyz;

    float NoL = dot(gData.worldNormal,sunDir);
    float NoLSafe = max(0.0f,NoL);

    float directShadow = evaluateDirectShadow(gData.worldPos,gData.worldNormal,NoLSafe);

    outHdrSceneColor.rgb = vec3(NoLSafe * directShadow + 0.05f) * gData.baseColor;
    outHdrSceneColor.a = 1.0f;

}