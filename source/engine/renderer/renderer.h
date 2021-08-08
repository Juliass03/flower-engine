#pragma once
#include "../core/runtime_module.h"
#include "imguiPass.h"

namespace engine{

class Renderer : public IRuntimeModule
{
private:

public:
	Renderer(Ref<ModuleManager>);
	
public:
	typedef void (RegisterImguiFunc)();

	virtual bool init() override;
	virtual void tick(float dt) override;
	virtual void release() override;

	void addImguiFunction(std::string name,const std::function<RegisterImguiFunc>& func)
	{
		this->m_uiFunctions[name] = func;
	}
	void removeImguiFunction(std::string name)
	{
		this->m_uiFunctions.erase(name);
	}

private:
	ImguiPass m_uiPass { };

private:
	void uiRecord();

	std::unordered_map<std::string,std::function<RegisterImguiFunc>> m_uiFunctions = {};
	bool show_demo_window = true;
	bool show_another_window = false;
};

}