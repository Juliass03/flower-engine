#pragma once
#include "../core/core.h"
#include <glm/glm.hpp>
#include "../vk/vk_rhi.h"

// NOTE: pmx 模型单独渲染
// 因为会有很多定制的风格化shadingModel.并且需要在cpu端解算IK和物理

namespace engine{

class PMXManager;
class PMXMeshComponent;

class PMXMaterial
{
	friend PMXManager;
public:
	std::string baseColorTextureName;
	uint32_t baseColorTextureId;

	std::string toonTextureName;
	uint32_t toonTextureId;

	std::string sphereTextureName;
	uint32_t sphereTextureId;

};

struct PMXSubMesh
{
	int32_t beginIndex;
	int32_t vertexCount;
	int32_t materialId;
};

inline std::vector<EVertexAttribute> getPMXMeshAttributes()
{
	return std::vector<EVertexAttribute>{
		EVertexAttribute::pos,
		EVertexAttribute::normal,
		EVertexAttribute::uv0
	};
}

// pmx mesh don't store vertexbuffer.
class PMXMesh
{
	friend PMXManager;
	friend PMXMeshComponent;
private:
	// vertex buffers.
	std::vector<float> m_positions; // vec3
	std::vector<float> m_normals;   // vec3
	std::vector<float> m_uvs;       // vec2

	

private:
	// index buffers.
	std::vector<uint32_t> m_indices;

private:
	std::vector<PMXMaterial> m_materials;
	std::vector<PMXSubMesh>  m_subMeshes;

private:
	VulkanIndexBuffer*  m_indexBuffer = nullptr;

public:
	~PMXMesh();
	VkDeviceSize getTotalVertexSize() const;
};

class PMXManager
{
private:
	std::unordered_map<std::string, PMXMesh*> m_cache;

public:
	static PMXManager* get();
	PMXMesh* getPMX(const std::string& name);

	void release();
};

}