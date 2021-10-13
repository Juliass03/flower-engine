#ifndef VIEW_DATA_GLSL
#define VIEW_DATA_GLSL

// 每个相机的数据
layout(set = 1, binding = 0) uniform ViewBuffer
{
	mat4 camView;
	mat4 camProj;
	mat4 camViewProj;
    vec4 camWorldPos; // .xyz为相机的世界空间坐标，.w=0.

} viewData;

#endif