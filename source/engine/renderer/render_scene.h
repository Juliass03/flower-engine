#pragma once
#include "scene_textures.h"
#include <memory>
#include "../scene/scene.h"
#include "../shader_compiler/shader_compiler.h"
#include "mesh.h"

/**
 * NOTE: RenderScene用来存储Renderable对象在World中的缓存
**/
namespace engine{

template<typename SSBOType>
struct SceneUploadSSBO;

struct GPUFrameData;
struct GPUObjectData;
struct GPUMaterialData;

class RenderScene
{
public:
	void initFrame(uint32 width,uint32 height,bool forceAllocateTextures = false);
	void renderPrepare(const GPUFrameData& view);

	void init(class Renderer* renderer);
	void release();
	
	RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler);
	~RenderScene();

	SceneTextures& getSceneTextures() { return *m_sceneTextures; }

	void uploadMeshSSBO();

	RenderMeshPack m_cacheStaticMeshRenderMesh;

	SceneUploadSSBO<GPUObjectData>* m_meshObjectSSBO;
	std::vector<GPUObjectData>   m_cacheMeshObjectSSBOData {};

	SceneUploadSSBO<GPUMaterialData>* m_meshMaterialSSBO;
	std::vector<GPUMaterialData> m_cacheMeshMaterialSSBOData {};

	std::vector<VkDrawIndexedIndirectCommand> m_cacheIndirectCommands {};
	struct DrawIndirectBuffer
	{
		VulkanBuffer* drawIndirectSSBO;
		VulkanDescriptorSetReference descriptorSets = {};
		VulkanDescriptorLayoutReference descriptorSetLayout = {};
		VkDeviceSize size;
		void init(uint32 bindingPos);
		void release();
	};
	
	DrawIndirectBuffer m_drawIndirectSSBOGbuffer;
	

	Scene& getActiveScene();

private:
	void allocateSceneTextures(uint32 width,uint32 height,bool forceAllocate = false);
	void meshCollect();

private:
	SceneTextures* m_sceneTextures;
	Renderer* m_renderer;
	Ref<SceneManager> m_sceneManager;
	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
};

}