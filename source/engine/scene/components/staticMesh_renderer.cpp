#include "staticmesh_renderer.h"
#include "../scene.h"
#include "../../renderer/mesh.h"
#include "../../shader_compiler/shader_compiler.h"

using namespace engine;

engine::StaticMeshComponent::StaticMeshComponent()
	: Component("StaticMeshComponent")
{

}

RenderMesh engine::StaticMeshComponent::getRenderMesh(Ref<shaderCompiler::ShaderCompiler> shaderCompiler)
{
	if(auto node = m_node.lock())
	{
		Mesh& mesh = MeshLibrary::get()->getMeshByName(m_meshName);
		auto& renderBounds = mesh.renderBounds;

		auto transform = node->getComponent<Transform>();
		RenderMesh ret {};

		// NOTE: ������SceneManager Tick�ж��Ѿ����������Transformˢ������
		//       ��˴�ʱ���Զ��̵߳ĸ���Mesh�ռ�
		ret.modelMatrix = transform->getWorldMatrix();
		ret.mesh = &mesh;

		return ret;
	}
}

void engine::StaticMeshComponent::switchMesh(const std::string& name)
{
	m_meshName = name;


}
