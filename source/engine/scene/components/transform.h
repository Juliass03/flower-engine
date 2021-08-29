#pragma once
#include <glm/gtx/matrix_decompose.hpp>
#include "../component.h"
#include "../../core/core.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp> 
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/archives/json.hpp>

namespace engine{

class SceneNode;
class Transform : public Component
{
private:
	std::weak_ptr<SceneNode> m_node;

    glm::vec3 m_translation { .0f,.0f,.0f };
    glm::quat m_rotation { 1.f, .0f, .0f, .0f };
    glm::vec3 m_scale { 1.f, 1.f, 1.f };
    glm::mat4 m_worldMatrix { 1.f };
	bool m_updateWorldMatrix { false };

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( cereal::base_class<Component>(this),
			cereal::defer(m_node),
			cereal::make_nvp("Translation",m_translation),
			cereal::make_nvp("Rotation",m_rotation),
			cereal::make_nvp("Scale",m_scale),
			cereal::make_nvp("WorldMatrix",m_worldMatrix),
			cereal::make_nvp("bUpdateWorldMatrix",m_updateWorldMatrix)
		);
	}

private:
	void updateWorldTransform();

public:
	Transform() {

	}
    Transform(std::shared_ptr<SceneNode> node): Component("Transform"), m_node(node) { }
    virtual ~Transform() = default;
	std::shared_ptr<SceneNode> getNode() { return m_node.lock(); }
    virtual size_t getType() override { return EComponentType::Transform; }
	void invalidateWorldMatrix(){ m_updateWorldMatrix = true; }

    void setTranslation(const glm::vec3& translation)
    {
        m_translation = translation;
		invalidateWorldMatrix();
    }

	void setRotation(const glm::quat& rotation)
	{
		m_rotation = rotation;
		invalidateWorldMatrix();
	}

	void setScale(const glm::vec3& scale)
	{
		m_scale = scale;
		invalidateWorldMatrix();
	}

	const auto& getTranslation() const
	{
		return m_translation;
	}

	const auto& getRotation() const
	{
		return m_rotation;
	}

	const auto& getScale() const
	{
		return m_scale;
	}

	void setMatrix(const glm::mat4& matrix)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(matrix, m_scale, m_rotation, m_translation, skew, perspective);
		m_rotation = glm::conjugate(m_rotation);
	}

	glm::mat4 getMatrix() const
	{
		// TRS - style
		return glm::translate(glm::mat4(1.0f), m_translation) *
			glm::mat4_cast(m_rotation) *
			glm::scale(glm::mat4(1.0f), m_scale);
	}

	// Lazy update transform.
	glm::mat4 getWorldMatrix()
	{
		updateWorldTransform();
		return m_worldMatrix;
	}
};



}

CEREAL_REGISTER_TYPE_WITH_NAME(engine::Transform, "Transform");
CEREAL_REGISTER_POLYMORPHIC_RELATION(engine::Component, engine::Transform)