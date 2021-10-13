#version 460
#define FRAGMENT_SHADER

#include "../glsl/common.glsl"
#include "../glsl/frame_data.glsl"
#include "../glsl/view_data.glsl"
#include "../glsl/pass_lighting.glsl"

void main()
{
    vec4 packGbufferBaseColorMetal = texture(inGbufferBaseColorMetal,inUV0);
    vec4 packGbufferNormalRoughness = texture(inGbufferNormalRoughness,inUV0);
    vec4 packGbufferEmissiveAo = texture(inGbufferEmissiveAo,inUV0);
    float deviceDepth = texture(inDepth,inUV0).r;

    vec3 baseColor = packGbufferBaseColorMetal.rgb;
    float metal = packGbufferBaseColorMetal.a;
    vec3 worldNormal = packGbufferNormalRoughness.rgb * 2.0f - vec3(1.0f);
    float roughness = packGbufferNormalRoughness.a;
    vec3 emissive = packGbufferEmissiveAo.rgb;
    float ao = packGbufferEmissiveAo.a;

    vec3 sunDir = normalize(-frameData.sunLightDir.xyz);
    vec3 sunColor = frameData.sunLightColor.xyz;

    float NoL = dot(worldNormal,sunDir);
    float NoLSafe = max(0.0f,NoL);

    // TODO: Lighting.
    outHdrSceneColor.rgb =vec3(NoLSafe + 0.05f) * baseColor;
    outHdrSceneColor.a = 1.0f;
}