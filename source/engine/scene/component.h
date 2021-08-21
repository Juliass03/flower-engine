#pragma once
#include <string>
#include <rttr/type>

namespace engine{

class Component
{
private:
	std::string m_name;

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

	virtual rttr::type getType() = 0;
};

}