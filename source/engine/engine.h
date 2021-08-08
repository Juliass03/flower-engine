#pragma once
#include "core/core.h"
#include "core/runtime_module.h"
#include <glfw/glfw3.h>
#include "core/windowData.h"
#include "core/timer.h"

namespace engine{

enum class ETickResult: uint8
{
	Continue,
	End,
};

class Engine final : private NonCopyable
{
private:
	std::unique_ptr<ModuleManager> m_moduleManager { nullptr };

public:
	Engine() = default;
	~Engine();

	template <typename T> 
	Ref<T> getRuntimeModule() const
	{
		return m_moduleManager->getRuntimeModule<T>();
	}

	bool init();
	ETickResult tick(float trueDt,float smoothDt);
	void release();
};

}