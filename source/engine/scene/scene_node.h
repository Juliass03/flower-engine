#pragma once
#include "components/transform.h"
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp> 
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/list.hpp>

namespace engine{

class SceneNode :public std::enable_shared_from_this<SceneNode>
{
    friend class Scene;
private:
    std::string m_name = "EmptyNode";
    size_t m_id;
    size_t m_depth = 0;
    
    // Ref
    std::shared_ptr<SceneNode> m_parent = nullptr;
    std::vector<std::shared_ptr<SceneNode>> m_children;

    // Owner
    std::shared_ptr<Transform> m_transform;
    std::unordered_map<size_t, std::shared_ptr<Component>> m_components;

private:
    friend class cereal::access;

    friend class cereal::access;
    template <class Archive>
    void serialize(Archive & ar)
    {
        ar( 
            cereal::make_nvp("SceneNode", m_name),
            cereal::make_nvp("NodeId", m_id),
            cereal::make_nvp("Depth", m_depth),
            cereal::make_nvp("Parent",m_parent),
            cereal::make_nvp("Children",m_children),
            cereal::make_nvp("Components", m_components),
            m_transform
        );
    }

    void updateDepth()
    {
        m_depth = m_parent->m_depth + 1;
        for(auto& child : m_children)
        {
            child->updateDepth();
        }
    }

public:
    // deprecated. use SceneNode::create
    SceneNode() = default;
    
    // deprecated. use SceneNode::create
    SceneNode(const size_t id,const std::string& name)
        : m_id(id),m_name(name),m_transform(nullptr)
    {
        LOG_TRACE("SceneNode {0} with GUID {1} create.", m_name.c_str(),m_id);
    }

    static std::shared_ptr<SceneNode> create(const size_t id,const std::string& name)
    {
        auto res = std::make_shared<SceneNode>(id,name);
        res->m_transform = std::make_shared<Transform>(res);
        res->setComponent(res->m_transform);
        return res;
    }

    virtual ~SceneNode()
    {
        LOG_TRACE("SceneNode {0} with GUID {1} destroy.", m_name.c_str(),m_id);
    }
    const auto getId() const{ return m_id;}
    const std::string& getName() const{ return m_name; }
    const auto& getChildren() const{ return m_children;}

public:
    std::shared_ptr<Component> getComponent(const size_t index) { return m_components.at(index);}
    
    template <class T> std::shared_ptr<T> getComponent();
    std::shared_ptr<Transform> getTransform(){ return m_transform; }
    std::shared_ptr<SceneNode> getParent() const{ return m_parent; }
    auto getDepth()
    {
        return m_depth;
    }
    // 判断节点是否为该节点的某个子节点
    bool isSon(std::shared_ptr<SceneNode> p)
    {
        std::shared_ptr<SceneNode> pp = p->m_parent;
        while(pp)
        {
            if(pp->getId() == m_id)
            {
                return true;
            }
            pp = pp->m_parent;
        }
        return false;
    }

    std::shared_ptr<SceneNode> getPtr()
    {
        return shared_from_this();
    }

    template <class T> bool hasComponent(){ return hasComponent(getTypeId<T>());}
    bool hasComponent(const size_t index){ return m_components.count(index) > 0;}
    void setName(const std::string& in)
    {
        m_name = in;
    }
private:
    friend class Scene;

    void addChild(std::shared_ptr<SceneNode> child)
    {  
        m_children.push_back(child);
    }

    // 禁止循环设置父级
    void setParent(std::shared_ptr<SceneNode> p)
    {
        m_parent = p; 
        updateDepth(); // 更新深度
        m_transform->invalidateWorldMatrix(); 
    }

    void selfDelete()
    {
        // std::shared_ptr<SceneNode> tt = getPtr();
        for(auto& child:m_children) 
        {
            child->selfDelete();
        }

        if(m_parent)
        {
            m_parent->removeChild(getPtr());
        }
    }

    void removeChild(std::shared_ptr<SceneNode> o)
    {
        std::vector<std::shared_ptr<SceneNode>>::iterator it;

        size_t id = 0;
        while(m_children[id]->getId()!=o->getId())
        {
            id ++; // 找到唯一的id 
        }

        std::swap(m_children[id],m_children[m_children.size() - 1]);
        m_children.pop_back();
    }

    void setComponent(std::shared_ptr<Component> component)
    {
        auto it = m_components.find(component->getType());

        if (it != m_components.end())
        {
            it->second = component;
        }
        else
        {
            m_components.insert(std::make_pair(component->getType(), component));
        }
    }

    template<class T>
    void removeComponent()
    {
        m_components.erase(getTypeId<T>());
    }
    
};

template<class T>
inline std::shared_ptr<T> SceneNode::getComponent()
{
    return std::dynamic_pointer_cast<T>(getComponent(getTypeId<T>()));
}

}