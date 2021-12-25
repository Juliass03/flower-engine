#pragma once
#include "../core/core.h"

// NOTE: pmx 模型单独渲染
// 因为会有很多定制的风格化shadingModel.并且需要在cpu端解算IK和物理

namespace engine{

struct PMXMaterial
{
	glm::vec3 diffuse;
	glm::vec3 specular;

	float     alpha;
	float     specularPower;

	glm::vec3 ambient;
};

struct PMXSubMesh
{
	int32_t beginIndex;
	int32_t vertexCount;
	int32_t materialId;
};

class PMXMesh
{
private:
	// vertex buffers.
	std::vector<glm::vec3> m_positions;
	std::vector<glm::vec3> m_normals;
	std::vector<glm::vec2> m_uvs;

private:
	// index buffers.
	std::vector<uint8_t> m_indices;
	size_t				 m_indexCount;
	size_t				 m_indexElementSize;

private:
	std::vector<PMXMaterial> m_materials;
	std::vector<PMXSubMesh>  m_subMeshes;

public:
	
};


}