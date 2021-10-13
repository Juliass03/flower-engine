#pragma once
#include "scene_textures.h"
#include <memory>
#include "../scene/scene.h"
#include "../shader_compiler/shader_compiler.h"

/**
 * NOTE: RenderScene�����洢Renderable������World�еĻ���
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

	void init(class Renderer* renderer);
	void release();
	
	RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler);
	~RenderScene();

	SceneTextures& getSceneTextures() { return *m_sceneTextures; }

	void uploadGbufferSSBO();

	std::vector<struct RenderMesh> m_cacheStaticMeshRenderMesh;
	SceneUploadSSBO* m_gbufferSSBO;
	std::vector<GPUObjectData> m_cacheGBufferObjectSSBOData {};

	Scene& getActiveScene();

private:
	void allocateSceneTextures(uint32 width,uint32 height,bool forceAllocate = false);
	void meshCollect(bool bRebuildMaterial);

private:
	SceneTextures* m_sceneTextures;
	Renderer* m_renderer;
	Ref<SceneManager> m_sceneManager;
	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
};

}