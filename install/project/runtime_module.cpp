#include "runtime_module.h"

namespace engine{

ModuleManager::~ModuleManager()
{
	// 从后往前重置
	for (size_t i = m_runtimeModules.size() - 1; i > 0; i--)
	{
		m_runtimeModules[i].ptr.reset();
	}

	m_runtimeModules.clear();
}

bool ModuleManager::init()
{
	bool result = true;

	// 按照注册顺序调用初始化函数
	for(const auto& runtimeModule : m_runtimeModules)
	{
		if(!runtimeModule.ptr->init())
		{
			LOG_ERROR("Runtime module {0} failed to init.",typeid(*runtimeModule.ptr).name());
			result = false;
		}
	}
	return result;
}

void ModuleManager::tick(ETickType type,float dt)
{
	// 按顺序Tick
	for(const auto& runtimeModule : m_runtimeModules)
	{
		if (runtimeModule.tickType != type)
			continue;

		runtimeModule.ptr->tick(dt);
	}
}

void ModuleManager::release()
{
	// 从后往前释放
	for (size_t i = m_runtimeModules.size(); i > 0; i--)
	{
		m_runtimeModules[i - 1].ptr->release();
	}
}

}