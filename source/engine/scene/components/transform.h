#pragma once
#include "../component.h"
#include "../../core/core.h"

namespace engine{

class SceneNode;

class Transform : public Component
{
	friend class Scene;
private:
	std::weak_ptr<SceneNode> m_node;
	bool  bUpdateFlag = true;

    glm::vec3 m_translation { .0f,.0f,.0f };
    glm::quat m_rotation = glm::quat(1.0, 0.0, 0.0, 0.0);
    glm::vec3 m_scale { 1.f, 1.f, 1.f };
	glm::mat4 m_worldMatrix = glm::mat4(1.0);
	

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( cereal::base_class<Component>(this),
			m_node,
			cereal::make_nvp("Translation",m_translation),
			cereal::make_nvp("Rotation",m_rotation),
			cereal::make_nvp("Scale",m_scale),
			cereal::make_nvp("WorldMatrix",m_worldMatrix)
		);
	}

private:
	void updateWorldTransform();

public:
	Transform() {}
    Transform(std::shared_ptr<SceneNode> node);
    virtual ~Transform() = default;
	std::shared_ptr<SceneNode> getNode();
    virtual size_t getType() override{ return EComponentType::Transform; }
	void invalidateWorldMatrix();
    void setTranslation(const glm::vec3& translation);
	void setRotation(const glm::quat& rotation);
	void setScale(const glm::vec3& scale);
	const glm::vec3& getTranslation() const;
	const glm::quat& getRotation() const;
	const glm::vec3& getScale() const;
	void setMatrix(const glm::mat4& matrix);
	glm::mat4 getMatrix() const;

	// Lazy update transform.
	glm::mat4 getWorldMatrix();
};

template<>
inline size_t getTypeId<Transform>()
{
	return EComponentType::Transform;
}
}

CEREAL_REGISTER_TYPE_WITH_NAME(engine::Transform, "Transform");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::Transform)