#pragma once
#include "widget.h"

class DockSpace : public Widget
{
public:
	DockSpace(engine::Ref<engine::Engine> engine);
	virtual void onVisibleTick(size_t) override;
	~DockSpace();

	

private:
	void showDockSpace();
	engine::Ref<engine::SceneManager> m_sceneManager;
};