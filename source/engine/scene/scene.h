#pragma once
#include "../core/core.h"
#include <rttr/type>

namespace engine{

class SceneNode;
class Component;
class Scene final
{
	using ComponentContainer = std::unordered_map<rttr::type,std::vector<std::unique_ptr<Component>>>;

private:
	std::string m_name;
	std::vector<std::unique_ptr<SceneNode>> m_nodes = {};
	SceneNode* m_root = nullptr;
	ComponentContainer m_components = {};

public:
	Scene();
	Scene(const std::string& name);
	~Scene();

	void setName(const std::string& name) { m_name = name; }
	const auto& getName() const { return m_name; }

	void setNodes(std::vector<std::unique_ptr<SceneNode>>&& nodes);
	void addNode(std::unique_ptr<SceneNode>&& node);
	void addChild(SceneNode& child);
	void addComponent(std::unique_ptr<Component>&& component);
	void addComponent(std::unique_ptr<Component>&& component, SceneNode& node);

	const std::vector<std::unique_ptr<Component>>& GetComponents(const rttr::type& type_info) const
	{
		return m_components.at(type_info);
	}

	bool hasComponent(const rttr::type& type_info) const
	{
		auto component = m_components.find(type_info);
		return (component != m_components.end() && !component->second.empty());
	}

	SceneNode* findNode(const std::string &name);
	SceneNode& getRootNode();

	template <class T>
	void setComponents(std::vector<std::unique_ptr<T>>&& components)
	{
		std::vector<std::unique_ptr<Component>> result(components.size());
		std::transform(components.begin(), components.end(), result.begin(),
			[](std::unique_ptr<T> &component) -> std::unique_ptr<Component> 
			{
				return std::unique_ptr<Component>(std::move(component));
			});

		setComponents(rttr::type::get<T>(), std::move(result));
	}

	void setComponents(const rttr::type& type_info, std::vector<std::unique_ptr<Component>>&& components)
	{
		m_components[type_info] = std::move(components);
	}

	template <class T>
	void clearComponents()
	{
		setComponents(rttr::type::get<T>(), {});
	}

	template <class T> std::vector<T*> getComponents() const
	{
		std::vector<T *> result;
		if (hasComponent(rttr::type::get<T>()))
		{
			auto& scene_components = getComponents(rttr::type::get<T>());

			result.resize(scene_components.size());
			std::transform(scene_components.begin(), scene_components.end(), result.begin(),
				[](const std::unique_ptr<Component> &component) -> T* 
				{
					return dynamic_cast<T*>(component.get());
				});
		}
		return result;
	}

	template <class T> bool hasComponent() const
	{
		return hasComponent(rttr::type::get<T>());
	}
};

}