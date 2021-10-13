#pragma once
#include "../component.h"
#include "../scene.h"

namespace engine{

class DirectionalLight : public Component
{
private:
	std::string m_lightName = "";
	std::weak_ptr<SceneNode> m_node;

public:
	glm::vec4 m_color = glm::vec4(1.0f,1.0f,1.0f,1.0f);

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar(cereal::base_class<Component>(this),
			m_color,
			m_lightName,
			m_node
		);
	}

public:
	DirectionalLight();
	~DirectionalLight();
	virtual size_t getType() override { return EComponentType::DirectionalLight; }
	void setNode(std::shared_ptr<SceneNode> node);

	glm::vec4 getDirection();
	glm::vec4 getColor();
	void setColor(glm::vec4);
};

template<>
inline size_t getTypeId<DirectionalLight>()
{
	return EComponentType::DirectionalLight;
}

template<>
inline void Scene::addComponent<DirectionalLight>(std::shared_ptr<DirectionalLight> component,std::shared_ptr<SceneNode> node)
{
	if(component&&!node->hasComponent(getTypeId<DirectionalLight>()))
	{
		node->setComponent(component);
		m_components[component->getType()].push_back(component);
		component->setNode(node);
	}
}
}


CEREAL_REGISTER_TYPE_WITH_NAME(engine::DirectionalLight, "DirectionalLight");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::DirectionalLight)