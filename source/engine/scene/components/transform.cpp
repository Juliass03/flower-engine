#include "transform.h"
#include "../scene_node.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine{

void Transform::updateWorldTransform()
{
	if(bUpdateFlag)
	{
		m_worldMatrixCache = m_worldMatrix;
		m_worldMatrix = getMatrix();
		
		auto parent = getNode()->getParent();

		if(parent)
		{
			auto transform = parent->getComponent<Transform>();
			m_worldMatrix = transform->getWorldMatrix() * m_worldMatrix;
		}

		bUpdateFlag = !bUpdateFlag;
	}
}

Transform::Transform(std::shared_ptr<SceneNode> node)
	: Component("Transform"), m_node(node) 
{
	
}

std::shared_ptr<SceneNode> Transform::getNode()
{ 
	return m_node.lock(); 
}

void Transform::invalidateWorldMatrix()
{ 
	bUpdateFlag = true; 

	auto& children = getNode()->getChildren();
	for(auto& child : children)
	{
		child->getTransform()->invalidateWorldMatrix();
	}
}

void Transform::setTranslation(const glm::vec3& translation)
{
	m_translation = translation;
	invalidateWorldMatrix();
}

void Transform::setRotation(const glm::quat& rotation)
{
	m_rotation = rotation;
	invalidateWorldMatrix();
}

void Transform::setScale(const glm::vec3& scale)
{
	m_scale = scale;
	invalidateWorldMatrix();
}

const glm::vec3& Transform::getTranslation() const
{
	return m_translation;
}

const glm::quat& Transform::getRotation() const
{
	return m_rotation;
}

const glm::vec3& Transform::getScale() const
{
	return m_scale;
}

void Transform::setMatrix(const glm::mat4& matrix)
{
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(matrix,m_scale,m_rotation,m_translation,skew,perspective);
	m_rotation = glm::conjugate(m_rotation);
}

glm::mat4 Transform::getMatrix() const
{
	// TRS - style
	glm::mat4 scale = glm::scale(m_scale);
	glm::mat4 rotator = glm::mat4_cast(m_rotation);
	glm::mat4 translate = glm::translate(glm::mat4(1.0f),m_translation);
	return translate * rotator * scale;
}

glm::mat4 Transform::getWorldMatrix()
{
	return m_worldMatrix;
}

glm::mat4 Transform::getPreWorldMatrix() const
{
	return m_worldMatrixCache;
}



}
