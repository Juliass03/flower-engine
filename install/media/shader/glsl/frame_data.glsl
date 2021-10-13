#ifndef FRAME_DATA_GLSL
#define FRAME_DATA_GLSL

// 每一帧公用的数据
layout(set = 0, binding = 0) uniform FrameBuffer
{
    vec4 appTime; // .x为app runtime，.y为game runtime, .z 为sin(game runtime), .w为cos(game runtime)

	vec4 sunLightDir;   // 直射灯方向
    vec4 sunLightColor; // 光照强度

} frameData;

#endif