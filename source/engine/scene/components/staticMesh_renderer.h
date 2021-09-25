#pragma once
#include "../component.h"
#include "../scene.h"

namespace engine{

namespace shaderCompiler
{
	class ShaderCompiler;
}

class Transform;
struct RenderMesh;
struct Mesh;

class StaticMeshComponent : public Component
{
public:
	std::string m_meshName = "";
	std::weak_ptr<SceneNode> m_node;

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( cereal::base_class<Component>(this),
			m_node,
			cereal::make_nvp("MeshName",m_meshName)
		);
	}

public:
	StaticMeshComponent();
	virtual ~StaticMeshComponent(){ };
	virtual size_t getType() override { return EComponentType::StaticMeshComponent; }
	RenderMesh getRenderMesh(Ref<shaderCompiler::ShaderCompiler> shaderCompiler);

	void switchMesh(const std::string& name);
};

template<>
inline size_t getTypeId<StaticMeshComponent>()
{
	return EComponentType::StaticMeshComponent;
}

template<>
inline void Scene::addComponent<StaticMeshComponent>(std::shared_ptr<StaticMeshComponent> component,std::shared_ptr<SceneNode> node)
{
	if(component && !node->hasComponent(getTypeId<StaticMeshComponent>()))
	{
		node->setComponent(component);
		m_components[component->getType()].push_back(component);
		component->m_node = node;
	}
}

}

CEREAL_REGISTER_TYPE_WITH_NAME(engine::StaticMeshComponent, "StaticMeshComponent");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::StaticMeshComponent)