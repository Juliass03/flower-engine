#ifndef COMMON_MESH_GLSL
#define COMMON_MESH_GLSL

layout(set = 1, binding = 0) uniform sampler2D BindlessSampler2D[];

vec4 texlod(uint id,vec2 uv, float lod)
{
    return textureLod(BindlessSampler2D[nonuniformEXT(id)], uv, lod);
}

vec4 tex(uint id,vec2 uv)
{
    return texture(BindlessSampler2D[nonuniformEXT(id)],uv);
}

layout(set = 2, binding = 0) readonly buffer PerObjectBuffer
{
	PerObjectData objects[];
} perObjectBuffer;

layout(set = 3, binding = 0) readonly buffer PerObjectMaterial
{
	PerObjectMaterialData materials[];
} perObjectMaterial;

layout(set = 4, binding = 0, std430) readonly buffer DrawIndirectBuffer
{
	IndexedIndirectCommand indirectDraws[];
} drawIndirectBuffer;

#endif