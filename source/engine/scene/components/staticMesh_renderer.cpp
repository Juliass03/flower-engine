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

RenderMesh engine::StaticMeshComponent::getRenderMesh(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,Ref<Renderer> renderer,bool bRebuildMaterial)
{
	if(auto node = m_node.lock())
	{
		Mesh& mesh = getMesh();

		auto transform = node->getComponent<Transform>();
		RenderMesh ret {};

		// NOTE: 我们在SceneManager Tick中对已经所有脏掉的Transform刷新坐标
		//       因此此时可以多线程的更新Mesh收集
		ret.modelMatrix = transform->getWorldMatrix();
		ret.mesh = &mesh;

		if(m_materials.size() != mesh.subMeshes.size())
		{
			reflectMaterials();
		}

		auto* materialLib = renderer->getMaterialLibrary();
		uint32 index = 0;
		for(auto& subMesh : mesh.subMeshes)
		{
			if(m_materials[index] != "")
			{
				// 注册材质
				subMesh.cacheMaterial = materialLib->registerMaterial(
					shaderCompiler,
					m_materials[index]
				);

				subMesh.cacheMaterial->getMaterialDescriptorSet(renderer->getMeshpassLayout(),bRebuildMaterial);
			}
			index++;
		}
		return ret;
	}
	else
	{
		LOG_FATAL("Miss node!");
		return RenderMesh();
	}
}

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
