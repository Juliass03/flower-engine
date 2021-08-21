#include "engine.h"
#include "core/timer.h"
#include "core/runtime_module.h"
#include "renderer/renderer.h"
#include "asset_system/asset_system.h"

namespace engine{

bool Engine::init()
{
	m_moduleManager = std::make_unique<ModuleManager>();
	m_moduleManager->m_engine = this;

	// 注册模块
	// NOTE: 注册顺序决定Tick调用次序和析构释放次序
	m_moduleManager->registerRuntimeModule<Renderer>(ETickType::True);
	m_moduleManager->registerRuntimeModule<asset_system::AssetSystem>(ETickType::True);

	// 初始化所有的模块
	m_moduleManager->init();


	return true;
}

Engine::~Engine()
{
	m_moduleManager.reset();
}

ETickResult Engine::tick(float trueDt,float smoothDt)
{
	m_moduleManager->tick(ETickType::True,     trueDt);
	m_moduleManager->tick(ETickType::Smoothed, smoothDt);

	return ETickResult::Continue;
}

void Engine::release()
{
	m_moduleManager->release();
}

}