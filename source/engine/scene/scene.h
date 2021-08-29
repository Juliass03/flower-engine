#pragma once
#include "../core/core.h"
#include <rttr/type>
#include "../core/runtime_module.h"
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp> 
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/list.hpp>
#include "component.h"
#include <list>

namespace engine{

// Scene 中直接存储各个SceneNode

// Scene 中存储了各类Component的弱引用
// 

namespace usageNode
{
	constexpr size_t empty = 0;
	constexpr size_t root = 1;


	constexpr size_t start = 100;
}

class SceneNode;
class Scene final
{
	// TODO: 生命周期管理
	using ComponentContainer = std::unordered_map<size_t,std::vector<std::weak_ptr<Component>>>; // TODO: 弱引用存储的容器

private:
	std::string m_name = "Untitled";
	size_t m_CurrentId = usageNode::start;

	// 场景树根节点
	std::shared_ptr<SceneNode> m_root;

	// 除开Transform外的所有Component
	ComponentContainer m_components = {};

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar( 
			cereal::make_nvp("Scene", m_name),
			cereal::make_nvp("Root",m_root),
			m_CurrentId,
			cereal::defer(m_components)
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

	size_t getLastGUID() const { return m_CurrentId; }

	void setName(const std::string& name) { m_name = name; }
	const auto& getName() const { return m_name; }

	std::shared_ptr<SceneNode> createNode(const std::string& name);

	void addChild(std::shared_ptr<SceneNode> child);
	void addComponent(std::shared_ptr<Component> component);
	void addComponent(std::shared_ptr<Component> component, SceneNode& node);

	// 获取
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
	}

	void setComponents(const size_t type_info, std::vector<std::weak_ptr<Component>>&& components)
	{
		m_components[type_info] = std::move(components);
	}

	template <class T>
	void clearComponents()
	{
		setComponents(getTypeId<T>(), {});
	}

	template <class T> const std::vector<std::shared_ptr<T>>& getComponents() const
	{
		std::vector<std::shared_ptr<T>> result;
		auto id = getTypeId<T>();
		if (hasComponent(id))
		{
			const auto& scene_components = getComponents(id);

			result.resize(scene_components.size());
			std::transform(scene_components.begin(), scene_components.end(), result.begin(),
				[](const std::shared_ptr<Component> &component) -> std::shared_ptr<T>
				{
					return std::dynamic_pointer_cast<T>(component.get());
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
	bool loadScene();
	bool unloadScene();

	Scene& getActiveScene()
	{
		return *m_activeScene;
	}

private:
	std::unique_ptr<Scene> createEmptyScene();

private:
	std::unique_ptr<Scene> m_activeScene = nullptr;
};

}