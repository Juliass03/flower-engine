#version 460

#define FRAGMENT_SHADER

#include "../glsl/common.glsl"
#include "../glsl/frame_data.glsl"
#include "../glsl/view_data.glsl"
#include "../glsl/pass_gbuffer.glsl"

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
    vec4 baseColorTex = texture(BaseColorTexture,inUV0);
    float mipmapLevel = textureQueryLod(BaseColorTexture, inUV0).x;
    float mipmapDitherFactor = min(0.8f, mipmapLevel / MAX_MIPMAP);
    if(baseColorTex.a < (0.99f - mipmapDitherFactor))
    {
        discard;
    }

    vec4 specularTex = texture(SpecularTexture,inUV0);
    vec4 emissiveTex = texture(EmissiveTexture,inUV0);

    outGbufferBaseColorMetal.rgb = baseColorTex.rgb;
    outGbufferBaseColorMetal.a   = specularTex.b; // metal

    vec3 normalTangentSpace = texture(NormalTexture,inUV0).xyz;
    normalTangentSpace.xy = normalTangentSpace.xy * 2.0f - vec2(1.0f);
    normalTangentSpace.y *= -1.0f;
    normalTangentSpace.z = sqrt(max(0.0f,1.0f - dot(normalTangentSpace.xy,normalTangentSpace.xy)));

    outGbufferNormalRoughness.rgb = (getNormalFromVertexAttribute(normalTangentSpace) + vec3(1.0f)) * 0.5f ;
    outGbufferNormalRoughness.a = specularTex.g; // roughness

    outGbufferEmissiveAo.rgb = emissiveTex.rgb; 
    outGbufferEmissiveAo.a = specularTex.r;  
}