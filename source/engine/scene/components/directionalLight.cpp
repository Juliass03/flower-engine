#include "directionalLight.h"
#include <glm/gtx/matrix_decompose.hpp>

using namespace engine;

engine::DirectionalLight::DirectionalLight()
	: Component("DirectionalLight")
{

}

engine::DirectionalLight::~DirectionalLight()
{
}

void engine::DirectionalLight::setNode(std::shared_ptr<SceneNode> node)
{
	m_node = node;
}

glm::vec4 engine::DirectionalLight::getDirection()
{
	if(auto node = m_node.lock())
	{
		const auto worldMatrix = node->getTransform()->getWorldMatrix();
		glm::vec4 forward = glm::vec4(0.0f,0.0f,1.0f,0.0f);
		return worldMatrix * forward;
	}

	return glm::vec4(0.0f);
}

glm::vec4 engine::DirectionalLight::getColor()
{
	return m_color;
}

void engine::DirectionalLight::setColor(glm::vec4 newColor)
{
	m_color = newColor;
}
