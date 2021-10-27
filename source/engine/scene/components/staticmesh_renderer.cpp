#include "staticmesh_renderer.h"
#include "../../renderer/mesh.h"
#include "../../shader_compiler/shader_compiler.h"
#include "../../renderer/renderer.h"
#include "../../renderer/material.h"

using namespace engine;

engine::StaticMeshComponent::StaticMeshComponent()
	: Component("StaticMeshComponent")
{

}

Mesh& engine::StaticMeshComponent::getMesh()
{
	Mesh* mesh;

	if(m_customMesh)
	{
		mesh = &MeshLibrary::get()->getMeshByName(m_customMeshName);
	}
	else
	{
		mesh = &MeshLibrary::get()->getMeshByName(m_meshName);
	}

	return *mesh;
}

std::vector<RenderSubMesh> engine::StaticMeshComponent::getRenderMesh(Ref<Renderer> renderer)
{
	if(auto node = m_node.lock())
	{
		std::vector<RenderSubMesh> ret {};

		Mesh& mesh = getMesh();

		auto transform = node->getComponent<Transform>();

		CHECK(mesh.indexCount > 0 && mesh.indexStartPosition >= 0);
		CHECK(mesh.vertexCount > 0 && mesh.vertexStartPosition >= 0);

		glm::mat4 modelMatrix = transform->getWorldMatrix();

		if(m_materials.size() != mesh.subMeshes.size())
		{
			// 若还没有反射材质先反射一次
			reflectMaterials();
		}

		// TODO: Parallel for
		uint32 index = 0;
		if(MeshLibrary::get()->MeshReady(mesh))
		{
			for(auto& subMesh : mesh.subMeshes)
			{
				RenderSubMesh renderSubMesh{};

				renderSubMesh.bCullingResult = true;
				renderSubMesh.indexCount = subMesh.indexCount;
				renderSubMesh.indexStartPosition = subMesh.indexStartPosition;
				renderSubMesh.renderBounds = subMesh.renderBounds;
				renderSubMesh.modelMatrix = modelMatrix;

				if(m_materials[index] != "")
				{
					renderSubMesh.cacheMaterial = MaterialLibrary::get()->getMaterial(m_materials[index]);
				}
				else
				{
					renderSubMesh.cacheMaterial = &MaterialLibrary::get()->getCallbackMaterial();
				}

				index++;
				ret.push_back(renderSubMesh);
			}
		}

		return ret;
	}
	else
	{
		LOG_FATAL("Miss node!");
		return {};
	}
}

// NOTE: 从资产中读取材质数据并反射
void engine::StaticMeshComponent::reflectMaterials()
{
	Mesh& mesh = getMesh();

	m_materials.resize(mesh.subMeshes.size());
	int32 index = 0;
	for(auto& subMesh : mesh.subMeshes)
	{
		m_materials[index] = subMesh.materialInfoPath;
		index++;
	}
}
