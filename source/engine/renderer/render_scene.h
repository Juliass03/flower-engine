#pragma once
#include "scene_textures.h"
#include <memory>
#include "../scene/scene.h"
#include "../shader_compiler/shader_compiler.h"

/**
 * NOTE: RenderScene用来存储Renderable对象在World中的缓存
**/
namespace engine{

struct SceneUploadSSBO;

struct GPUObjectData
{
	__declspec(align(16)) glm::mat4 model;
};

class RenderScene
{
public:
	void initFrame(uint32 width,uint32 height,bool forceAllocateTextures = false);

	void init();
	void release();
	
	RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler);
	~RenderScene();

	SceneTextures& getSceneTextures() { return *m_sceneTextures; }

	void uploadGbufferSSBO();

	std::vector<struct RenderMesh> m_cacheStaticMeshRenderMesh;
	SceneUploadSSBO* m_gbufferSSBO;
	std::vector<GPUObjectData> m_cacheGBufferObjectSSBOData {};

private:
	void allocateSceneTextures(uint32 width,uint32 height,bool forceAllocate = false);
	void meshCollect();

private:
	SceneTextures* m_sceneTextures;
	Ref<SceneManager> m_sceneManager;
	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
};

}