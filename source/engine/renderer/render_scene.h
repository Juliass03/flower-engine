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

constexpr auto CASCADE_MAX_COUNT = 4u;

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
	bool isSceneEmpty();

	RenderMeshPack m_cacheStaticMeshRenderMesh;

	SceneUploadSSBO<GPUObjectData>* m_meshObjectSSBO;
	std::vector<GPUObjectData>   m_cacheMeshObjectSSBOData {};

	SceneUploadSSBO<GPUMaterialData>* m_meshMaterialSSBO;
	std::vector<GPUMaterialData> m_cacheMeshMaterialSSBOData {};

	struct DrawIndirectBuffer
	{
		VulkanBuffer* drawIndirectSSBO;
		VulkanDescriptorSetReference descriptorSets = {};
		VulkanDescriptorLayoutReference descriptorSetLayout = {};
		VkDeviceSize size;

		void init(uint32 bindingPos,uint32 countBindingPos);
		void release();

		// Count Buffer
		VulkanBuffer* countBuffer;
		VulkanDescriptorSetReference countDescriptorSets = {};
		VulkanDescriptorLayoutReference countDescriptorSetLayout = {};
		VkDeviceSize countSize;
	};
	
	DrawIndirectBuffer m_drawIndirectSSBOGbuffer;
	
	std::array<DrawIndirectBuffer,CASCADE_MAX_COUNT> m_drawIndirectSSBOShadowDepths {};

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