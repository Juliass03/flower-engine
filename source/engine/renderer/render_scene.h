#pragma once
#include "scene_textures.h"
#include <memory>


/**
 * NOTE: RenderScene用来存储Renderable对象在World中的缓存
**/
namespace engine{

class RenderScene
{
public:
	void initFrame(uint32 width,uint32 height);

	void init();
	void release();
	
	RenderScene();
	~RenderScene();

	SceneTextures& getSceneTextures() { return *m_sceneTextures; }
private:
	void allocateSceneTextures(uint32 width,uint32 height);

private:
	SceneTextures* m_sceneTextures;
};

}