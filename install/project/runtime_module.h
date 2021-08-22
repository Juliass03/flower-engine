#pragma once
#include <memory>
#include <type_traits>
#include "core.h"

namespace engine{

class ModuleManager;
class Engine;

class IRuntimeModule : private NonCopyable
{
protected:
	Ref<ModuleManager> m_moduleManager;

public:
	IRuntimeModule(Ref<ModuleManager> in) : m_moduleManager(in) { }
	virtual ~IRuntimeModule() = default;

	virtual bool init() { return true; }
	virtual void tick(float dt) = 0;
	virtual void release() {  }
};

template<typename Worker>
constexpr void checkRuntimeModule() 
{ 
	static_assert(std::is_base_of<IRuntimeModule, Worker>::value, "This type doesn't match runtime module."); 
}

enum class ETickType
{
	True,    // Use true delta time to tick.
	Smoothed // Use smoothed delta time to tick.
};

class ModuleManager : private NonCopyable
{
	friend Engine;
private:
	struct RuntimeModule
	{
		RuntimeModule(std::unique_ptr<IRuntimeModule> runtimeModule,ETickType type)
		: tickType(type)
		{
			ptr = std::move(runtimeModule);
		}

		std::unique_ptr<IRuntimeModule> ptr;
		ETickType tickType;
	};

	std::vector<RuntimeModule> m_runtimeModules;
	Ref<Engine> m_engine;
public:
	~ModuleManager();

	Ref<Engine> getEngine() { return m_engine; }

	template<typename Worker>
	void registerRuntimeModule(ETickType type = ETickType::True)
	{
		checkRuntimeModule<Worker>();
		RuntimeModule activeModule(std::make_unique<Worker>(this),type);
		m_runtimeModules.push_back(std::move(activeModule));
	}

	bool init();
	void tick(ETickType,float dt = 0.0f);
	void release();

	template <typename Worker> 
	Ref<Worker> getRuntimeModule() const
	{
		// NOTE: 在此处便检查了类型Worker是否继承自IRuntimeModule
		//       因此下方的类型转换使用static_cast而不是dynamic_cast
		checkRuntimeModule<Worker>();
		for (const auto& runtimeModule : m_runtimeModules)
		{
			if (runtimeModule.ptr && typeid(Worker) == typeid(*runtimeModule.ptr))
			{
				return static_cast<Ref<Worker>>(runtimeModule.ptr.get());
			}
		}
		return nullptr;
	}
};

}