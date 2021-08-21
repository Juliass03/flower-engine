#pragma once
#include <glm/gtx/matrix_decompose.hpp>
#include "../component.h"

namespace engine{

class SceneNode;
class Transform : public Component
{
private:
	SceneNode& m_node;

    glm::vec3 m_translation { .0f,.0f,.0f };
    glm::quat m_rotation { 1.f, .0f, .0f, .0f };
    glm::vec3 m_scale { 1.f, 1.f, 1.f };
    glm::mat4 m_worldMatrix { 1.f };

	bool m_updateWorldMatrix { false };

	void updateWorldTransform();
public:
    Transform(SceneNode& node): Component("Transform"), m_node(node) { }
    virtual ~Transform() = default;
    SceneNode& getNode() { return m_node; }
    virtual rttr::type getType() override { return rttr::type::get<Transform>(); }
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