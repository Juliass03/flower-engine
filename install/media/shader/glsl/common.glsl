#ifndef COMMON_GLSL
#define COMMON_GLSL

const float g_pi = 3.14159265f;
const float g_max_mipmap = 12.0f;

struct PerObjectData   // SSBO upload each draw call mesh data
{
	mat4 model;         // model matrix

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
};

// 当前帧公用的数据
layout(set = 0, binding = 0) uniform FrameBuffer
{
    vec4 appTime;       // .x为app runtime，.y为game runtime, .z 为sin(game runtime), .w为cos(game runtime)

	vec4 sunLightDir;   // 直射灯方向
    vec4 sunLightColor; // 光照颜色

    mat4 camView;       // 相机视图矩阵
	mat4 camProj;       // 相机投影矩阵
	mat4 camViewProj;   // 相机视图投影矩阵
    vec4 camWorldPos;   // .xyz为相机的世界空间坐标，.w=0.
	vec4 camInfo;       // .x fovy .y aspect_ratio .z nearZ .w farZ
    vec4 camFrustumPlanes[6];


} frameData;

vec3 packGBufferNormal(vec3 inNormal)
{
    return (inNormal + vec3(1.0f,1.0f,1.0f)) * 0.5f;
}

vec3 unpackGbufferNormal(vec3 inNormal)
{
    return inNormal * 2.0f - vec3(1.0f,1.0f,1.0f);
}

#endif