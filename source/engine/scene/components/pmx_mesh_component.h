#pragma once
#include "../component.h"
#include "../scene.h"

namespace engine{

class PMXMeshComponent : public Component
{
private:
	std::string m_pmxPath = "";
	std::weak_ptr<SceneNode> m_node;

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar(cereal::base_class<Component>(this),
			m_pmxPath,
			m_node
		);
	}

public:
	PMXMeshComponent();
	~PMXMeshComponent();

	virtual size_t getType() override { return EComponentType::PMXMeshComponent; }
	void setNode(std::shared_ptr<SceneNode> node);
	std::string getPath() const;

	void setPath(std::string path);
};

template<>
inline size_t getTypeId<PMXMeshComponent>()
{
	return EComponentType::PMXMeshComponent;
}

template<>
inline void Scene::addComponent<PMXMeshComponent>(std::shared_ptr<PMXMeshComponent> component,std::shared_ptr<SceneNode> node)
{
	if(component&&!node->hasComponent(getTypeId<PMXMeshComponent>()))
	{
		node->setComponent(component);
		m_components[component->getType()].push_back(component);
		component->setNode(node);
	}
}
}


CEREAL_REGISTER_TYPE_WITH_NAME(engine::PMXMeshComponent, "PMXMeshComponent");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::PMXMeshComponent)