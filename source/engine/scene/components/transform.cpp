#include "transform.h"
#include "../scene_node.h"

namespace engine{

void Transform::updateWorldTransform()
{
	if (!m_updateWorldMatrix)
	{
		return;
	}

	m_worldMatrix = getMatrix();
	auto parent = m_node.getParent();

	if (parent)
	{
		auto &transform = parent->getComponent<Transform>();
		m_worldMatrix = m_worldMatrix * transform.getWorldMatrix();
	}

	m_updateWorldMatrix = false;
}

}