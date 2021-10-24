#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"

layout (set = 1, binding = 0) uniform sampler2D inGbufferBaseColorMetal;
layout (set = 1, binding = 1) uniform sampler2D inGbufferNormalRoughness;
layout (set = 1, binding = 2) uniform sampler2D inGbufferEmissiveAo;
layout (set = 1, binding = 3) uniform sampler2D inDepth;

layout (location = 0) in  vec2 inUV0;
layout (location = 0) out vec4 outHdrSceneColor;

struct GbufferData
{
    vec3 baseColor;
    vec3 emissiveColor;
    vec3 worldNormal;

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

    return gData;
}

void main()
{
    GbufferData gData = loadGbufferData();

    vec3 sunDir = normalize(-frameData.sunLightDir.xyz);
    vec3 sunColor = frameData.sunLightColor.xyz;

    float NoL = dot(gData.worldNormal,sunDir);
    float NoLSafe = max(0.0f,NoL);

    outHdrSceneColor.rgb = vec3(NoLSafe + 0.05f) * gData.baseColor;
    outHdrSceneColor.a = 1.0f;
}