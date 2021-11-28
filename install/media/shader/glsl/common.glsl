#ifndef COMMON_GLSL
#define COMMON_GLSL

const float g_pi = 3.14159265f;
const float g_max_mipmap = 12.0f;

struct PerObjectData   // SSBO upload each draw call mesh data
{
	mat4 model;         // model matrix
    mat4 preModel;      // last frame model matrix

    vec4 sphereBounds;  // .xyz localspace center pos
                        // .w   sphere radius

    vec4 extents;       // .xyz extent XYZ
                        // .w   pad

    // indirect draw call data
    uint indexCount;    
	uint firstIndex;    
	uint vertexOffset;  
	uint firstInstance; 
};

struct PerObjectMaterialData // SSBO upload each draw call material data
{
    uint baseColorTexId;
    uint normalTexId;
    uint specularTexId;
    uint emissiveTexId;
};

struct IndexedIndirectCommand // Gpu culling result
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;

    uint objectId;
    uint materialId;
};

struct OutIndirectDrawCount
{
    uint outDrawCount;
};

struct CascadeInfo
{
    float splitPosition;
    mat4 cascadeViewProjMatrix;
};

vec3 packGBufferNormal(vec3 inNormal)
{
    return (inNormal + vec3(1.0f,1.0f,1.0f)) * 0.5f;
}

vec3 unpackGbufferNormal(vec3 inNormal)
{
    return inNormal * 2.0f - vec3(1.0f,1.0f,1.0f);
}

vec3 getWorldPosition(float z,vec2 uv,mat4 camInvViewProj)
{
    vec4 posClip  = vec4(uv * 2.0f - 1.0f, z, 1.0f);
    posClip.y *= -1.0f; // We use vulkan viewport flip y

    vec4 posWorld = camInvViewProj * posClip;
    return posWorld.xyz / posWorld.w;
}

// return [znear - zfar]
float linearizeDepth(float z,float n,float f)
{
    return n * f / (f + z * (n - f));
}

mat4 lookAtRH(vec3 eye,vec3 center,vec3 up)
{
    const vec3 f = normalize(center - eye);
    const vec3 s = normalize(cross(f, up));
    const vec3 u = cross(s, f);

    mat4 ret =  {
        {1.0f,0.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,1.0f,0.0f},
        {1.0f,0.0f,0.0f,1.0f}
    };

    ret[0][0] = s.x;
    ret[1][0] = s.y;
    ret[2][0] = s.z;
    ret[0][1] = u.x;
    ret[1][1] = u.y;
    ret[2][1] = u.z;
    ret[0][2] =-f.x;
    ret[1][2] =-f.y;
    ret[2][2] =-f.z;
    ret[3][0] =-dot(s, eye);
    ret[3][1] =-dot(u, eye);
    ret[3][2] = dot(f, eye);

    return ret;
}

mat4 lookAtLH(vec3 eye,vec3 center,vec3 up)
{
    vec3 f = (normalize(center - eye));
    vec3 s = (normalize(cross(up, f)));
    vec3 u = (cross(f, s));

    mat4 Result =  {
        {1.0f,0.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,1.0f,0.0f},
        {1.0f,0.0f,0.0f,1.0f}
    };
    Result[0][0] = s.x;
    Result[1][0] = s.y;
    Result[2][0] = s.z;
    Result[0][1] = u.x;
    Result[1][1] = u.y;
    Result[2][1] = u.z;
    Result[0][2] = f.x;
    Result[1][2] = f.y;
    Result[2][2] = f.z;
    Result[3][0] = -dot(s, eye);
    Result[3][1] = -dot(u, eye);
    Result[3][2] = -dot(f, eye);
    return Result;
}

mat4 orthoRH_ZO(float left, float right, float bottom, float top, float zNear, float zFar)
{
    mat4 ret =  {
        {1.0f,0.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,1.0f,0.0f},
        {1.0f,0.0f,0.0f,1.0f}
    };

    ret[0][0] = 2.0f / (right - left);
    ret[1][1] = 2.0f / (top - bottom);
    ret[2][2] = -1.0f / (zFar - zNear);
    ret[3][0] = -(right + left) / (right - left);
    ret[3][1] = -(top + bottom) / (top - bottom);
    ret[3][2] = -zNear / (zFar - zNear);

	return ret;
}

mat4 orthoLH_ZO(float left, float right, float bottom, float top, float zNear, float zFar)
{
    mat4 Result =  {
        {1.0f,0.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,1.0f,0.0f},
        {1.0f,0.0f,0.0f,1.0f}
    };
    Result[0][0] = 2.0f / (right - left);
    Result[1][1] = 2.0f / (top - bottom);
    Result[2][2] = 1.0f / (zFar - zNear);
    Result[3][0] = - (right + left) / (right - left);
    Result[3][1] = - (top + bottom) / (top - bottom);
    Result[3][2] = - zNear / (zFar - zNear);
    return Result;
}

uvec3 Rand3DPCG16(ivec3 p)
{
	uvec3 v = uvec3(p);
	v = v * 1664525u + 1013904223u;
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	return v >> 16u;
}

uint ReverseBits32(uint bits)
{
	bits = ( bits << 16) | ( bits >> 16);
	bits = ( (bits & 0x00ff00ff) << 8 ) | ( (bits & 0xff00ff00) >> 8 );
	bits = ( (bits & 0x0f0f0f0f) << 4 ) | ( (bits & 0xf0f0f0f0) >> 4 );
	bits = ( (bits & 0x33333333) << 2 ) | ( (bits & 0xcccccccc) >> 2 );
	bits = ( (bits & 0x55555555) << 1 ) | ( (bits & 0xaaaaaaaa) >> 1 );
	return bits;
}

vec2 Hammersley16( uint Index, uint NumSamples, uvec2 Random )
{
	float E1 = fract( float(Index) / float(NumSamples) + float(Random.x) * (1.0 / 65536.0));
	float E2 = float( ( ReverseBits32(Index) >> 16 ) ^ Random.y ) * (1.0 / 65536.0);
	return vec2( E1, E2 );
}

#endif