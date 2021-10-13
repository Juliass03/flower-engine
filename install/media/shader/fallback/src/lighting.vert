#version 460
#define VERTEX_SHADER

#include "../../glsl/common.glsl"
#include "../../glsl/frame_data.glsl"
#include "../../glsl/view_data.glsl"
#include "../../glsl/pass_lighting.glsl"

void main()
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}