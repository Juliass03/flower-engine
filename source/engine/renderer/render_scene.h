#pragma once
#include "scene_textures.h"
#include <memory>


/**
 * NOTE: RenderScene�����洢Renderable������World�еĻ���
**/
namespace engine{

class RenderScene
{
public:
	void initFrame(uint32 width,uint32 height);

	
	void release();
	
	RenderScene();
	~RenderScene();

private:
	void allocateSceneTextures(uint32 width,uint32 height);

private:
	SceneTextures* m_sceneTextures;
};

}