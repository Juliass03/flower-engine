#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"

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

	void setSceneViewportSize(uint32 width,uint32 height) { m_sceneViewportWidth = width; m_sceneViewportHeight = height; }

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
	RenderScene m_renderScene { };


private:
	void uiRecord();

	std::unordered_map<std::string,std::function<RegisterImguiFunc>> m_uiFunctions = {};
	bool show_demo_window = true;
	bool show_another_window = false;

	uint32 m_sceneViewportWidth = 0;
	uint32 m_sceneViewportHeight = 0;
};

}