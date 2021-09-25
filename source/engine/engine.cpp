#include "engine.h"
#include "core/timer.h"
#include "core/runtime_module.h"
#include "renderer/renderer.h"
#include "asset_system/asset_system.h"
#include "scene/scene.h"
#include "core/job_system.h"
#include "shader_compiler/shader_compiler.h"

namespace engine{

bool Engine::init()
{
	jobsystem::initialize();

	m_moduleManager = std::make_unique<ModuleManager>();
	m_moduleManager->m_engine = this;

	// ע��ģ��
	// NOTE: ע��˳�����Tick���ô���������ͷŴ���
	//       ��Ҫ���׵������ǵ�˳��
	m_moduleManager->registerRuntimeModule<asset_system::AssetSystem>(ETickType::True);
	m_moduleManager->registerRuntimeModule<shaderCompiler::ShaderCompiler>(ETickType::True);
	m_moduleManager->registerRuntimeModule<SceneManager>(ETickType::Smoothed);
	m_moduleManager->registerRuntimeModule<Renderer>(ETickType::True);

	// ��ʼ�����е�ģ��
	m_moduleManager->init();

	return true;
}

Engine::~Engine()
{
	m_moduleManager.reset();
	jobsystem::destroy();
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