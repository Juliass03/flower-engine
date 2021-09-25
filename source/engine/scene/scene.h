#pragma once
#include "../core/core.h"
#include "../core/runtime_module.h"
#include "component.h"
#include <list>
#include "scene_node.h"

namespace engine{

namespace usageNodeIndex
{
	constexpr size_t empty = 0;
	constexpr size_t root  = 1;
	constexpr size_t start = 100;
}

class Scene final
{
	using ComponentContainer = std::unordered_map<size_t,std::vector<std::weak_ptr<Component>>>; // TODO: 弱引用存储的容器

private:
	std::string m_name = "Untitled";
	size_t m_CurrentId = usageNodeIndex::start;

	// 场景树根节点
	std::shared_ptr<SceneNode> m_root;

	// 除开Transform外的所有Component
	ComponentContainer m_components = {};

	bool m_dirty = false;

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( 
			cereal::make_nvp("Scene", m_name),
			cereal::make_nvp("Root",m_root),
			m_CurrentId,
			m_components
		);
	}

	size_t requireId()
	{
		m_CurrentId ++; 
		return m_CurrentId;
	}

public:
	Scene();
	Scene(const std::string& name);
	~Scene();

	void setDirty(bool bDirty = true) { m_dirty = bDirty; }
	bool isDirty() const { return m_dirty; }

	void flushSceneNodeTransform();

	size_t getLastGUID() const { return m_CurrentId; }
	void deleteNode(std::shared_ptr<SceneNode> node);

	// NOTE: 自底向上更新
	void loopNodeDownToTop(const std::function<void(std::shared_ptr<SceneNode>)>& func, std::shared_ptr<SceneNode> node);

	// NOTE: 自顶向下更新
	void loopNodeTopToDown(const std::function<void(std::shared_ptr<SceneNode>)>& func, std::shared_ptr<SceneNode> node);


	void setName(const std::string& name) { m_name = name; setDirty(); }
	const auto& getName() const { return m_name; }

	std::shared_ptr<SceneNode> createNode(const std::string& name);

	// NOTE: 给根节点添加子节点
	void addChild(std::shared_ptr<SceneNode> child);

	template<typename T>
	inline void addComponent(std::shared_ptr<T> component,std::shared_ptr<SceneNode> node)
	{
		if (component && !node->hasComponent(getTypeId<T>()))
		{
			node->setComponent(component);
			m_components[getTypeId<T>()].push_back(component);
		}
	}

	template<> inline void addComponent<class StaticMeshComponent>(std::shared_ptr<StaticMeshComponent>,std::shared_ptr<SceneNode>);

	template<typename T>
	void removeComponent(std::shared_ptr<SceneNode> node)
	{
		if(node->hasComponent<T>())
		{
			setDirty();
			return node->removeComponent<T>();
		}
	}

	// NOTE: 设立父子关系
	bool setParent(std::shared_ptr<SceneNode> parent,std::shared_ptr<SceneNode> son);

	const std::vector<std::weak_ptr<Component>>& getComponents(const size_t type_info) const
	{
		return m_components.at(type_info);
	}

	bool hasComponent(size_t type_info) const
	{
		auto component = m_components.find(type_info);
		return (component != m_components.end() && !component->second.empty());
	}

	std::shared_ptr<SceneNode> findNode(const std::string &name);
	std::shared_ptr<SceneNode> getRootNode();

	template <class T>
	void setComponents(std::vector<std::weak_ptr<T>>&& components)
	{
		std::vector<std::weak_ptr<Component>> result(components.size());
		std::transform(components.begin(), components.end(), result.begin(),
			[](std::weak_ptr<T> &component) -> std::weak_ptr<Component> 
			{
				return std::weak_ptr<Component>(std::move(component));
			});

		setComponents(getTypeId<T>(), std::move(result));
		setDirty();
	}

	void setComponents(const size_t type_info, std::vector<std::weak_ptr<Component>>&& components)
	{
		m_components[type_info] = std::move(components);
		setDirty();
	}

	template <class T>
	void clearComponents()
	{
		setComponents(getTypeId<T>(), {});
		setDirty();
	}

	template <class T> 
	std::vector<std::weak_ptr<T>> getComponents() const
	{
		std::vector<std::weak_ptr<T>> result;
		auto id = getTypeId<T>();
		if (hasComponent(id))
		{
			const auto& scene_components = getComponents(id);

			result.resize(scene_components.size());
			std::transform(scene_components.begin(), scene_components.end(), result.begin(),
				[](const std::weak_ptr<Component> &component) -> std::weak_ptr<T>
				{
					return std::static_pointer_cast<T>(component.lock());
				});
		}
		return result;
	}

	template <class T> bool hasComponent() const
	{
		return hasComponent(getTypeId<T>());
	}
};

class SceneManager: public IRuntimeModule
{
public:
	SceneManager(Ref<ModuleManager>);

public:
	virtual bool init() override;
	virtual void tick(float dt) override;
	virtual void release() override;

public:
	bool loadScene(const std::string& path);
	bool unloadScene();
	bool saveScene(const std::string& path);

	Scene& getActiveScene()
	{
		return *m_activeScene;
	}

private:
	void createEmptyScene();
	std::string originalTile;
private:
	std::unique_ptr<Scene> m_activeScene = nullptr;
};

}