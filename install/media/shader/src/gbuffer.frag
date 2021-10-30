#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../glsl/common.glsl"
#include "../glsl/common_mesh.glsl"

layout (location = 0) in  vec3 inWorldNormal;
layout (location = 1) in  vec2 inUV0;
layout (location = 2) in  vec3 inWorldPos;
layout (location = 3) in  vec3 inViewPos;
layout (location = 4) in  vec4 inTangent;
layout (location = 5) in  flat uint InMaterialId;

layout (location = 0) out vec4 outGbufferBaseColorMetal; 
layout (location = 1) out vec4 outGbufferNormalRoughness; 
layout (location = 2) out vec4 outGbufferEmissiveAo; 

vec3 getNormalFromVertexAttribute(vec3 tangentVec3) 
{
    vec3 n = normalize(inWorldNormal);
    vec3 t = normalize(inTangent.xyz);
    t = normalize(t - dot(t, n) * n);
    vec3 b = normalize(cross(n,t) * inTangent.w);
    mat3 tbn =  mat3(t, b, n);
    return normalize(tbn * normalize(tangentVec3));
}

void main() 
{
    PerObjectMaterialData matData = perObjectMaterial.materials[nonuniformEXT(InMaterialId)];

    vec4 baseColorTex = tex(matData.baseColorTexId,inUV0);

    if(baseColorTex.a < 0.5f)
    {
        discard;
    }

    vec4 normalTex   = tex(matData.normalTexId,  inUV0);
    vec4 specularTex = tex(matData.specularTexId,inUV0);
    vec4 emissiveTex = tex(matData.emissiveTexId,inUV0);

    vec3 normalTangentSpace = normalTex.xyz;
    normalTangentSpace.xy = normalTangentSpace.xy * 2.0f - vec2(1.0f);
    normalTangentSpace.y *= -1.0f;
    normalTangentSpace.z = sqrt(max(0.0f,1.0f - dot(normalTangentSpace.xy,normalTangentSpace.xy)));
    vec3 worldSpaceNormal = getNormalFromVertexAttribute(normalTangentSpace);

    outGbufferBaseColorMetal.rgb = baseColorTex.rgb;
    outGbufferNormalRoughness.rgb = packGBufferNormal(worldSpaceNormal);
    outGbufferEmissiveAo.rgb = emissiveTex.rgb; 

    outGbufferBaseColorMetal.a  = specularTex.b; // metal
    outGbufferNormalRoughness.a = specularTex.g; // roughness
    outGbufferEmissiveAo.a      = specularTex.r; // ao
}