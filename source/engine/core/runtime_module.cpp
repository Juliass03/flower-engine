#include "runtime_module.h"

namespace engine{

ModuleManager::~ModuleManager()
{
	// �Ӻ���ǰ����
	for (size_t i = m_runtimeModules.size() - 1; i > 0; i--)
	{
		m_runtimeModules[i].ptr.reset();
	}

	m_runtimeModules.clear();
}

bool ModuleManager::init()
{
	bool result = true;

	// ����ע��˳����ó�ʼ������
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
	// ��˳��Tick
	for(const auto& runtimeModule : m_runtimeModules)
	{
		if (runtimeModule.tickType != type)
			continue;

		runtimeModule.ptr->tick(dt);
	}
}

void ModuleManager::release()
{
	// �Ӻ���ǰ�ͷ�
	for (size_t i = m_runtimeModules.size(); i > 0; i--)
	{
		m_runtimeModules[i - 1].ptr->release();
	}
}

}