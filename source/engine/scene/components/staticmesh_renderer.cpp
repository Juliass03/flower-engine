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

RenderMesh engine::StaticMeshComponent::getRenderMesh(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,Ref<Renderer> renderer)
{
	if(auto node = m_node.lock())
	{
		Mesh& mesh = getMesh();

		auto transform = node->getComponent<Transform>();

		RenderMesh ret {};

		CHECK(mesh.indexBuffer);
		CHECK(mesh.vertexBuffer);

		ret.indexBuffer = mesh.indexBuffer;
		ret.vertexBuffer = mesh.vertexBuffer;

		// TODO: 我们在SceneManager Tick中对已经所有脏掉的Transform刷新坐标
		//       因此此时可以多线程的更新Mesh收集
		ret.modelMatrix = transform->getWorldMatrix();

		if(m_materials.size() != mesh.subMeshes.size())
		{
			// 若还没有反射材质先反射一次
			reflectMaterials();
		}

		uint32 index = 0;
		for(auto& subMesh : mesh.subMeshes)
		{
			RenderSubMesh renderSubMesh{};

			renderSubMesh.bCullingResult = true;
			renderSubMesh.indexCount = subMesh.indexCount;
			renderSubMesh.indexStartPosition = subMesh.indexStartPosition;
			renderSubMesh.renderBounds = subMesh.renderBounds;
			if(m_materials[index] != "")
			{
				renderSubMesh.cacheMaterial = MaterialLibrary::get()->getMaterial(m_materials[index]);
			}
			else
			{
				renderSubMesh.cacheMaterial = &MaterialLibrary::get()->getCallbackMaterial();
			}

			index++;
			ret.submesh.push_back(renderSubMesh);
		}

		return ret;
	}
	else
	{
		LOG_FATAL("Miss node!");
		return RenderMesh();
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
