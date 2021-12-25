#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_shadow.glsl"
#include "../glsl/common_framedata.glsl"
#include "../glsl/brdf.glsl"

layout (set = 1, binding = 0) uniform sampler2D inGbufferBaseColorMetal;
layout (set = 1, binding = 1) uniform sampler2D inGbufferNormalRoughness;
layout (set = 1, binding = 2) uniform sampler2D inGbufferEmissiveAo;
layout (set = 1, binding = 3) uniform sampler2D inDepth;
layout (set = 1, binding = 4) uniform sampler2DArray inShadowDepthBilinearTexture;
layout (set = 1, binding = 5) uniform sampler2D inBRDFLut;
layout (set = 1, binding = 6) uniform samplerCube inIrradiancePrefilter;
layout (set = 1, binding = 7) uniform samplerCube inEnvSpecularPrefilter;

layout(set = 2, binding = 0) readonly buffer CascadeInfoBuffer
{
	CascadeInfo cascadeInfosbuffer[];
};

struct LightingPushConstants
{
	float pcfDilation;
    uint bReverseZ;
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
    vec3 worldNormalVertex;

    vec3 worldPos;

    float metal;
    float roughness;
    float ao;

    float deviceDepth;
    float linearZ; // 0 - 1
    float linearDepth;

    vec3 F0;

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
    gData.deviceDepth = clamp(sampleDeviceDepth,0.0f,1.0f);
    gData.linearDepth = linearizeDepth(gData.deviceDepth,frameData.camInfo.z,frameData.camInfo.w);
    gData.linearZ = gData.linearDepth / abs(frameData.camInfo.z - frameData.camInfo.w);
    gData.worldPos = getWorldPosition(gData.deviceDepth,inUV0,frameData.camInvertViewProjectionJitter);

    vec3 ddxWorldPos = normalize(dFdxFine(gData.worldPos));
    vec3 ddyWorldPos = normalize(dFdyFine(gData.worldPos));

    gData.worldNormalVertex = -normalize(cross(ddxWorldPos,ddyWorldPos));

    
    gData.F0 = mix(vec3(0.04f), gData.baseColor, vec3(gData.metal));

    return gData;
}

vec3 getCascadeDebugColor(uint index)
{
    if(index == 0)
    {
        return vec3(1.0f,0.0f,0.0f);
    }
    else if(index == 1)
    {
        return vec3(0.0f,1.0f,0.0f);
    }
    else if(index == 2)
    {
        return vec3(0.0f,0.0f,1.0f);
    }
    else
    {
        return vec3(1.0f,0.0f,1.0f);
    }
}

bool unValid(vec3 vector)
{
    return isnan(vector.x) || isnan(vector.y) || isnan(vector.z) || isinf(vector.x) || isinf(vector.y) || isinf(vector.z);
}

vec3 biasNormalOffset(vec3 vertexNormal, float NoLSafe, float bias, float texelSize)
{
    return vertexNormal * (1.0f - NoLSafe) * bias * texelSize * 10.0f;
}

float autoBias(float safeNoL,float biasMul, float bias)
{
    float fixedFactor = 0.0005f;
    float slopeFactor = 1.0f - safeNoL;

    return fixedFactor + slopeFactor * bias * biasMul * 0.0001f;
}

float evaluateDirectShadow(
    float linearZ, 
    float NoLSafe, 
    vec3 fragWorldPos,
    vec3 normal,
    out uint outIndex,
    vec3 lightDirection,
    vec3 vertexNormal)
{
    ivec2 texDim = textureSize(inShadowDepthBilinearTexture,0).xy;
	vec2 texelSize = 1.0f / vec2(texDim);

    float shadowFactor = 1.0f;
    const uint cascadeCount = 4;

    float VertexNoLSafe = clamp(dot(vertexNormal,lightDirection), 0.0f, 1.0f);

    const float biasFactor = 10.0f;
    
    vec3 worldPosProcess = fragWorldPos + biasNormalOffset(vertexNormal, VertexNoLSafe, biasFactor, texelSize.x);

    for(uint cascadeIndex = 0; cascadeIndex < cascadeCount; cascadeIndex ++)
	{
        vec4 shadowClipPos = cascadeInfosbuffer[cascadeIndex].cascadeViewProjMatrix * vec4(worldPosProcess, 1.0);
        vec4 shadowCoordNdc = shadowClipPos / shadowClipPos.w;
        vec4 shadowCoord = shadowCoordNdc;
		shadowCoord.st = shadowCoord.st * 0.5f + 0.5f;
        shadowCoord.y = 1.0f - shadowCoord.y;

        if( shadowCoord.x > 0.01f && shadowCoord.y > 0.01f && 
			shadowCoord.x < 0.99f && shadowCoord.y < 0.99f &&
			shadowCoord.z > 0.01f  && shadowCoord.z < 0.99f)
		{
            // const bool bReverseZOpen = pushConstants.bReverseZ != 0;
            const bool bReverseZOpen = true; // Now always reverse z
            
            const float pcfDilation = 2.0f;
            const float minPcfDialtion = 0.0f;
            const float pcssDilation = 40.0f;

            float bias = autoBias(VertexNoLSafe,cascadeIndex + 1, biasFactor);
            shadowCoord.z += bias;
            shadowFactor = shadowPcf(
                cascadeIndex,
                inShadowDepthBilinearTexture,
                shadowCoord,
                texelSize,
                pcfDilation,
                bReverseZOpen,
                NoLSafe
            );

            const float shadowCascadeBlendThreshold = 0.8f;
            vec2 posNdc = abs(shadowCoordNdc.xy);
            float cascadeFade = (max(posNdc.x,posNdc.y) - shadowCascadeBlendThreshold) * 4.0f;

            if (cascadeFade > 0.0f && cascadeIndex < cascadeCount - 1)
            {
                uint nextIndexCascade = cascadeIndex + 1;
                vec4 nextShadowClipPos = cascadeInfosbuffer[nextIndexCascade].cascadeViewProjMatrix * vec4(worldPosProcess, 1.0);
                vec4 nextShadowCoord = nextShadowClipPos / nextShadowClipPos.w;
                nextShadowCoord.st = nextShadowCoord.st * 0.5f + 0.5f;
                nextShadowCoord.y = 1.0f - nextShadowCoord.y;

                bias = autoBias(VertexNoLSafe,nextIndexCascade + 1, biasFactor);
                nextShadowCoord.z += bias;

                float nextShadowFactor = shadowPcf(
                    nextIndexCascade,
                    inShadowDepthBilinearTexture,
                    nextShadowCoord,
                    texelSize,
                    pcfDilation,
                    bReverseZOpen,
                    NoLSafe
                );

                shadowFactor = mix(shadowFactor, nextShadowFactor, cascadeFade);

                outIndex = cascadeIndex;
            }
            break;
        }
    }
    return shadowFactor;
}

void main()
{
    GbufferData gData = loadGbufferData();

    ivec2 texDim = textureSize(inGbufferBaseColorMetal,0).xy;
	vec2 texelSize = 1.0f / vec2(texDim);

    vec3 l = normalize(-frameData.sunLightDir.xyz);
    vec3 n = gData.worldNormal;
    vec3 v = normalize(frameData.camWorldPos.xyz - gData.worldPos);
    vec3 h = normalize(v + l);

    float NoL = max(0.0f,dot(n, l));
    float LoH = max(0.0f,dot(l, h));
    float VoH = max(0.0f,dot(v, h));
    float NoV = max(0.0f,dot(n, v));
    float NoH = max(0.0f,dot(n, h));
    vec3 Lr   = 2.0 * NoV * n - v; // 反射向量
    vec3 F0   = gData.F0; 

    uint cascadeIndex = 0;
    float directShadow = evaluateDirectShadow(
        gData.linearZ, 
        NoL, 
        gData.worldPos,
        n,
        cascadeIndex,
        l,
        gData.worldNormal
    );  
    vec3 debugColor = getCascadeDebugColor(cascadeIndex);
    directShadow = max(0.0,directShadow);

    vec3 lightRadiance = frameData.sunLightColor.xyz * directShadow * 3.14f;

    vec3 directLighting = vec3(0.0);
    {
        vec3 F    = FresnelSchlick(VoH, F0);
        float D = DistributionGGX(NoH, gData.roughness);   
        float G   = GeometrySmith(NoV, NoL, gData.roughness);      
        vec3 kD = mix(vec3(1.0) - F, vec3(0.0), gData.metal);

        vec3 diffuseBRDF = kD * gData.baseColor;
        vec3 specularBRDF = (F * D * G) / max(0.00001, 4.0 * NoL * NoV);

        directLighting += (diffuseBRDF + specularBRDF) * NoL;
    }
    directLighting *= lightRadiance;

    vec3 ambientLighting = vec3(0.0);
    {
		vec3 irradiance = texture(inIrradiancePrefilter, n).rgb;

		vec3 F = FresnelSchlick(NoV,F0);
		vec3 kd = mix(vec3(1.0) - F, vec3(0.0), gData.metal);
		vec3 diffuseIBL = kd * gData.baseColor * irradiance;

		int specularTextureLevels = textureQueryLevels(inEnvSpecularPrefilter);
		vec3 specularIrradiance = textureLod(inEnvSpecularPrefilter, Lr, gData.roughness * specularTextureLevels).rgb;
		vec2 specularBRDF = texture(inBRDFLut, vec2(NoV, gData.roughness)).rg;
		vec3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		ambientLighting = (diffuseIBL + specularIBL);
    }
    // ambientLighting *= lightRadiance;

    outHdrSceneColor.rgb = directLighting + ambientLighting + gData.emissiveColor * 3.14f;
    float rr = texture(inShadowDepthBilinearTexture,vec3(inUV0,0)).r;
    outHdrSceneColor.a = 1.0f;

    outHdrSceneColor.rgb = max(vec3(0.0f), outHdrSceneColor.rgb);
}