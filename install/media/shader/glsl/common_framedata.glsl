#ifndef COMMON_FRAMEDATAS_GLSL
#define COMMON_FRAMEDATAS_GLSL

// 当前帧公用的数据
layout(set = 0, binding = 0) uniform FrameBuffer
{
    uvec4 frameIndex; // .x frame count

    vec4 appTime;       // .x为app runtime，.y为game runtime, .z 为sin(game runtime), .w为cos(game runtime)

	vec4 sunLightDir;   // 直射灯方向
    vec4 sunLightColor; // 光照颜色

    mat4 camView;       // 相机视图矩阵
	mat4 camProj;       // 相机投影矩阵
	mat4 camViewProj;   // 相机视图投影矩阵 No TAA Jitter

    mat4 camInvertViewProjection; // No TAA Jitter

    mat4 camViewProjJitter; // With Jitter
    // With Jitter
    mat4 camInvertViewProjectionJitter;

    mat4 camViewProjLast; // 上一帧相机数据  No TAA Jitter
    vec4 jitterData; // .xy current jitter, .zw last jitter

    vec4 camWorldPos;   // .xyz为相机的世界空间坐标，.w=0.
	vec4 camInfo;       // .x fovy .y aspect_ratio .z nearZ .w farZ
    vec4 camFrustumPlanes[6];
} frameData;

#endif