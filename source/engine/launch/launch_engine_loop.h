#pragma once
#include "../core/core.h"
#include "../engine.h"

namespace engine{

class EngineLoop final
{
public:
	EngineLoop()  = default;
	~EngineLoop() = default; 

public:
	bool init();
	void guardedMain();
	void release();

	uint32 getFrameCount() const { return m_frameCount; }
	Ref<Engine> getEngine() { return &m_engine; }
	
	Ref<bool> getRun( ) { return &m_run; }
private:	
	Engine m_engine {};
	uint32 m_frameCount;
	float m_smooth_dt = 0.0f;
	bool m_run = true;
};

extern EngineLoop g_engineLoop;

}