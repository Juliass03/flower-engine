#include "scene.h"
#include "scene_node.h"
#include <queue>
#include <cereal/archives/json.hpp>
#include <ostream>
#include <fstream>

namespace engine{

Scene::Scene()
{
	m_root = SceneNode::create(usageNode::root,"Root");
}

Scene::Scene(const std::string& name) : m_name(name)
{
	m_root = SceneNode::create(usageNode::root,"Root");
}

Scene::~Scene()
{
	m_root.reset();
}

std::shared_ptr<SceneNode> Scene::createNode(const std::string& name)
{
	return SceneNode::create(requireId(),name);
}

void Scene::addChild(std::shared_ptr<SceneNode> child)
{
	m_root->addChild(child);
}

std::shared_ptr<SceneNode> Scene::getRootNode()
{
	return m_root;
}

void Scene::addComponent(std::shared_ptr<Component> component, SceneNode& node)
{
	node.setComponent(component);

	if (component)
	{
		m_components[component->getType()].push_back(component);
	}
}

void Scene::addComponent(std::shared_ptr<Component> component)
{
	if (component)
	{
		m_components[component->getType()].push_back(component);
	}
}

std::shared_ptr<SceneNode> Scene::findNode(const std::string &node_name)
{
	for (auto& root_node : m_root->getChildren())
	{
		std::queue<std::shared_ptr<SceneNode>> traverse_nodes{};
		traverse_nodes.push(root_node);

		while (!traverse_nodes.empty())
		{
			auto& node = traverse_nodes.front();
			traverse_nodes.pop();

			if (node->getName() == node_name)
			{
				return node;
			}

			for (auto& child_node : node->getChildren())
			{
				traverse_nodes.push(child_node);
			}
		}
	}
	return m_root;
}


SceneManager::SceneManager(Ref<ModuleManager> in)
: IRuntimeModule(in)
{
}

bool SceneManager::init()
{
	m_activeScene = createEmptyScene();

	return true;
}

void SceneManager::tick(float dt)
{
}

void SceneManager::release()
{
	if(m_activeScene) m_activeScene.reset();
}

bool SceneManager::loadScene()
{
	return true;
}

bool SceneManager::unloadScene()
{
	return true;
}

std::unique_ptr<Scene> SceneManager::createEmptyScene()
{
	CHECK(m_activeScene == nullptr);

	auto res = std::make_unique<Scene>("untitled");

	// Test. 
	std::ofstream os("my.json");
	cereal::JSONOutputArchive archive(os);
	archive(*res);

	return std::move(res);
}

}