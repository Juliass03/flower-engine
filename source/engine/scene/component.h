#pragma once
#include <string>
#include <rttr/type>
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp> 
#include <cereal/types/unordered_map.hpp>
#include "../core/core.h"
namespace engine{

namespace EComponentType
{
	constexpr int32_t Transform = 0;
}

class Transform;

template<typename T>
size_t getTypeId()
{
	if(typeid(T)==typeid(Transform))
	{
		return EComponentType::Transform;
	}

	return -1;
}

class Component
{
private:
	std::string m_name = "Component";

private:
	friend class cereal::access;

	template <class Archive>
	void serialize(Archive& ar)
	{
		ar(cereal::make_nvp("Component", m_name));
	}

public:
	Component() = default;
	virtual ~Component() = default;

	Component(const std::string& name)
		: m_name(name)
	{

	}

	Component(std::string&& name)
		: m_name(std::move(name))
	{

	}

	Component (Component&& other)
		: m_name(other.m_name) 
	{

	}

	const auto& getName() const
	{
		return m_name;
	}

	virtual size_t getType() = 0;
};

}