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

		// TODO: ������SceneManager Tick�ж��Ѿ����������Transformˢ������
		//       ��˴�ʱ���Զ��̵߳ĸ���Mesh�ռ�
		ret.modelMatrix = transform->getWorldMatrix();

		if(m_materials.size() != mesh.subMeshes.size())
		{
			// ����û�з�������ȷ���һ��
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

// NOTE: ���ʲ��ж�ȡ�������ݲ�����
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
