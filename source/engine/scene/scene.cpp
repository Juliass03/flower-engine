#include "scene.h"
#include "scene_node.h"
#include <queue>

namespace engine{

Scene::Scene()
{
	m_root = new SceneNode(0,"Root");
}

Scene::Scene(const std::string& name) : m_name(name)
{
	m_root = new SceneNode(0,"Root");
}

Scene::~Scene()
{
	delete m_root;
}

void Scene::setNodes(std::vector<std::unique_ptr<SceneNode>>&& n)
{
	ASSERT(m_nodes.empty(),"Scene nodes are no empty! so setNodes() will no work!");
	m_nodes = std::move(n);
}

void Scene::addNode(std::unique_ptr<SceneNode>&& n)
{
	m_nodes.emplace_back(std::move(n));
}

void Scene::addChild(SceneNode& child)
{
	m_root->addChild(child);
}

SceneNode& Scene::getRootNode()
{
	return *m_root;
}

void Scene::addComponent(std::unique_ptr<Component>&& component, SceneNode& node)
{
	node.setComponent(*component);

	if (component)
	{
		m_components[component->getType()].push_back(std::move(component));
	}
}

void Scene::addComponent(std::unique_ptr<Component> &&component)
{
	if (component)
	{
		m_components[component->getType()].push_back(std::move(component));
	}
}

SceneNode* Scene::findNode(const std::string &node_name)
{
	for (auto root_node : m_root->getChildren())
	{
		std::queue<SceneNode*> traverse_nodes{};
		traverse_nodes.push(root_node);

		while (!traverse_nodes.empty())
		{
			auto node = traverse_nodes.front();
			traverse_nodes.pop();

			if (node->getName() == node_name)
			{
				return node;
			}

			for (auto child_node : node->getChildren())
			{
				traverse_nodes.push(child_node);
			}
		}
	}
	return nullptr;
}


}