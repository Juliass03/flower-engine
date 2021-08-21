#pragma once
#include "components/transform.h"

namespace engine{

class SceneNode
{
private:
    size_t m_id;
    std::string m_name;
    Transform m_transform;
    SceneNode* m_parent{ nullptr };
    std::vector<SceneNode*> m_children;
    std::unordered_map<rttr::type, Component*> m_components;

public:
    SceneNode(const size_t id,const std::string& name)
        : m_id(id),m_name(name),m_transform(*this)
    {
        setComponent(m_transform);
    }

    virtual ~SceneNode() = default;
    const auto getId() const{ return m_id;}
    const std::string& getName() const{ return m_name; }
    Transform& getTransform(){ return m_transform; }
    SceneNode* getParent() const{ return m_parent; }
    const std::vector<SceneNode*>& getChildren() const{ return m_children;}

    template <class T> T& getComponent(){ return dynamic_cast<T&>(getComponent(rttr::type::get<T>()));}
    Component& getComponent(const rttr::type index) { return *m_components.at(index);}
    template <class T> bool hasComponent(){ return hasComponent(rttr::type::get<T>());}
    bool hasComponent(const rttr::type index){ return m_components.count(index) > 0;}
    void addChild(SceneNode& child){  m_children.push_back(&child);}
    void setParent(SceneNode& p) { m_parent = &p; m_transform.invalidateWorldMatrix(); }

    void setComponent(Component& component)
    {
        auto it = m_components.find(component.getType());

        if (it != m_components.end())
        {
            it->second = &component;
        }
        else
        {
            m_components.insert(std::make_pair(component.getType(), &component));
        }
    }
};

 }