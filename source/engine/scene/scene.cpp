#include "scene.h"
#include "scene_node.h"
#include <queue>
#include <cereal/archives/json.hpp>
#include <ostream>
#include <fstream>
#include <glfw/glfw3.h>
#include "../core/windowData.h"
#include "../core/file_system.h"
#include "components/sceneview_camera.h"

namespace engine{

Scene::Scene()
{
	m_root = SceneNode::create(usageNodeIndex::root,"Root");
	addSceneViewCameraNode();
}

Scene::Scene(const std::string& name) : m_name(name)
{
	m_root = SceneNode::create(usageNodeIndex::root,"Root");
	addSceneViewCameraNode();
}

Scene::~Scene()
{
	m_root.reset();
}

void Scene::flushSceneNodeTransform()
{
	loopNodeTopToDown([](std::shared_ptr<SceneNode> node)
	{
		node->m_transform->updateWorldTransform();
	},m_root);
}

void Scene::addSceneViewCameraNode()
{
	m_sceneViewCameraNode = createNode("SceneViewCamera");

	auto cameraComponent = std::make_shared<SceneViewCamera>();
	addComponent<SceneViewCamera>(cameraComponent,m_sceneViewCameraNode);
}

void Scene::deleteNode(std::shared_ptr<SceneNode> node)
{
	setDirty();
	node->selfDelete();
}

void Scene::loopNodeDownToTop(const std::function<void(std::shared_ptr<SceneNode>)>& func,std::shared_ptr<SceneNode> node)
{
	auto& children = node->getChildren();

	for(auto& child : children)
	{
		loopNodeDownToTop(func,child);
	}

	func(node);
}

void Scene::loopNodeTopToDown(const std::function<void(std::shared_ptr<SceneNode>)>& func,std::shared_ptr<SceneNode> node)
{
	func(node);

	auto& children = node->getChildren();

	for(auto& child : children)
	{
		loopNodeTopToDown(func,child);
	}
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

std::shared_ptr<SceneNode> Scene::getSceneViewCameraNode()
{
	return m_sceneViewCameraNode;
}

bool Scene::setParent(std::shared_ptr<SceneNode> parent,std::shared_ptr<SceneNode> son)
{
	bool bNeedSet = false;

	auto oldP = son->getParent();
	if(oldP == nullptr || !son->isSon(parent) || parent->getId() != oldP->getId())
	{
		bNeedSet = true;
	}

	if(bNeedSet)
	{
		son->setParent(parent);
		parent->addChild(son);

		if(oldP) oldP->removeChild(son);

		setDirty();
		return true;
	}

	return false;
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
	createEmptyScene();
	originalTile = *(CVarSystem::get()->getStringCVar("r.Window.TileName"));

	return true;
}

void SceneManager::tick(float dt)
{
	std::string sceneTile;
	if(m_activeScene->isDirty())
	{
		sceneTile = originalTile + " - " + m_activeScene->getName() + " *";
	}
	else
	{
		sceneTile = originalTile+ " - " + m_activeScene->getName();
	}

	static std::string currentTile = originalTile;

	if(currentTile != sceneTile)
	{
		currentTile = sceneTile;
		glfwSetWindowTitle(g_windowData.window, currentTile.c_str());
	}

	// NOTE: 先刷新所有脏节点的Transform，便于后面的多线程收集。
	m_activeScene->flushSceneNodeTransform();
}

void SceneManager::release()
{
	if(m_activeScene) m_activeScene.reset();
}

bool SceneManager::loadScene(const std::string& path)
{
	if(!FileSystem::endWith(path,s_projectSuffix))
	{
		LOG_ERROR("Loading scene path {0} no end with scene suffix!",path);
		return false;
	}

	std::ifstream os(path);
	cereal::JSONInputArchive iarchive(os);
	std::unique_ptr<Scene> loadScene = std::make_unique<Scene>();
	iarchive(*loadScene);

	m_activeScene = std::move(loadScene);

	return true;
}

bool SceneManager::unloadScene()
{
	m_activeScene.reset();
	return true;
}

bool SceneManager::saveScene(const std::string& path)
{
	if(!FileSystem::endWith(path,s_projectSuffix))
	{
		LOG_ERROR("Saving scene path {0} no end with scene suffix!",path);
		return false;
	}

	std::ofstream os(path);
	cereal::JSONOutputArchive archive(os);

	m_activeScene->setName(FileSystem::getFileNameWithoutSuffix(path));

	archive(*m_activeScene);
	m_activeScene->setDirty(false);

	return true;
}

void SceneManager::createEmptyScene()
{
	CHECK(m_activeScene == nullptr);

	m_activeScene = std::make_unique<Scene>("untitled");
	m_activeScene->setDirty(false);
}

}