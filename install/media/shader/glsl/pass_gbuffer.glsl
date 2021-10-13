#ifndef PASS_GBUFFER_GLSL
#define PASS_GBUFFER_GLSL

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

// 确保gbuffer的透明度纹理和depth的位置名称一致便于统一创建
layout (set = 3,binding = 0) uniform sampler2D BaseColorTexture;
layout (set = 3,binding = 1) uniform sampler2D NormalTexture;
layout (set = 3,binding = 2) uniform sampler2D SpecularTexture;
layout (set = 3,binding = 3) uniform sampler2D EmissiveTexture;

#ifdef VERTEX_SHADER
	layout (location = 0) in vec3 inPosition;
	layout (location = 1) in vec2 inUV0;
	layout (location = 2) in vec3 inNormal;
	layout (location = 3) in vec4 inTangent;
	layout (location = 0) out vec3 outWorldNormal;
	layout (location = 1) out vec2 outUV0;
	layout (location = 2) out vec3 outWorldPos;
	layout (location = 3) out vec3 outViewPos;
	layout (location = 4) out vec4 outTangent;
#endif

#ifdef FRAGMENT_SHADER
	layout (location = 0) in  vec3 inWorldNormal;
	layout (location = 1) in  vec2 inUV0;
	layout (location = 2) in  vec3 inWorldPos;
	layout (location = 3) in  vec3 inViewPos;
	layout (location = 4) in  vec4 inTangent;
	layout (location = 0) out vec4 outGbufferBaseColorMetal; 
	layout (location = 1) out vec4 outGbufferNormalRoughness; 
	layout (location = 2) out vec4 outGbufferEmissiveAo; 
#endif

#endif