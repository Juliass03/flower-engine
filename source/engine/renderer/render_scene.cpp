#include "render_scene.h"

namespace engine{

RenderScene::RenderScene()
{
	m_sceneTextures = new SceneTextures();
}

RenderScene::~RenderScene()
{
	delete m_sceneTextures;
}

void RenderScene::initFrame(uint32 width,uint32 height)
{
	// 1. 首先申请合适的SceneTextures
	allocateSceneTextures(width,height);


}

void RenderScene::allocateSceneTextures(uint32 width,uint32 height)
{
	m_sceneTextures->allocate(width,height);
}

void RenderScene::release()
{
	m_sceneTextures->release();
}


}
