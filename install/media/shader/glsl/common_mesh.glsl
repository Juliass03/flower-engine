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

struct PerObjectData
{
	mat4 model;
};

layout(std140, set = 2, binding = 0) readonly buffer PerObjectBuffer
{
	PerObjectData objects[];
} perObjectBuffer;

struct PerObjectMaterialData
{
    uint baseColorTexId;
    uint normalTexId;
    uint specularTexId;
    uint emissiveTexId;
};

layout(set = 3, binding = 0) readonly buffer PerObjectMaterial
{
	PerObjectMaterialData materials[];
} perObjectMaterial;

#endif