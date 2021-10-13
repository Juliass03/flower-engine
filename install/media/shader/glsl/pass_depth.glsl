#ifndef PASS_DEPTH_GLSL
#define PASS_DEPTH_GLSL

// GBufferPass 公用的数据
struct ObjectData
{
	mat4 model;
};

// std140 强制内存对齐
layout(std140, set = 2, binding = 0) readonly buffer ObjectBuffer
{
	ObjectData objects[];
} objectBuffer;

// NOTE: MaskTexture
layout (set = 3,binding = 0) uniform sampler2D BaseColorTexture;

#ifdef VERTEX_SHADER
	layout (location = 0) in vec3 inPosition;
	layout (location = 1) in vec2 inUV0;
	layout (location = 2) in vec3 inNormal;
	layout (location = 3) in vec4 inTangent;
	layout (location = 0) out vec3 outWorldNormal;
	layout (location = 1) out vec2 outUV0;
	layout (location = 2) out vec3 outWorldPos;
#endif

#ifdef FRAGMENT_SHADER
	layout (location = 0) in  vec3 inWorldNormal;
	layout (location = 1) in  vec2 inUV0;
	layout (location = 2) in  vec3 inWorldPos;
    layout (location = 0) out vec4 outColor;
#endif

#endif