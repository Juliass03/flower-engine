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

class PMXMeshComponent;

class RenderScene
{
public:
	void initFrame(uint32 width,uint32 height,bool forceAllocateTextures = false);
	void renderPrepare(const GPUFrameData& view, VkCommandBuffer cmd);

	void init(class Renderer* renderer);
	void release();
	
	RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler);
	~RenderScene();

	SceneTextures& getSceneTextures() { return *m_sceneTextures; }

	void uploadMeshSSBO();
	bool isSceneStaticMeshEmpty();

	RenderMeshPack m_cacheStaticMeshRenderMesh;

	SceneUploadSSBO<GPUObjectData>* m_meshObjectSSBO;
	std::vector<GPUObjectData>   m_cacheMeshObjectSSBOData {};

	SceneUploadSSBO<GPUMaterialData>* m_meshMaterialSSBO;
	std::vector<GPUMaterialData> m_cacheMeshMaterialSSBOData {};

	std::vector<std::weak_ptr<PMXMeshComponent>> m_cachePMXMeshComponents {};

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

	struct EvaluateDepthMinMaxBuffer
	{
		// 评估场景的最大最小深度(用于Cascade视锥构建)
		VulkanBuffer* buffer; 
		VulkanDescriptorSetReference descriptorSets = {};
		VulkanDescriptorLayoutReference descriptorSetLayout = {};
		VkDeviceSize size;

		void init(uint32 bindingPos);
		void release();
	};
	EvaluateDepthMinMaxBuffer m_evaluateDepthMinMax;

	struct CascadeSetupBuffer
	{
		// 构建Cascade阴影信息
		VulkanBuffer* buffer; 
		VulkanDescriptorSetReference descriptorSets = {};
		VulkanDescriptorLayoutReference descriptorSetLayout = {};
		VkDeviceSize size;

		void init(uint32 bindingPos);
		void release();
	};
	CascadeSetupBuffer m_cascadeSetupBuffer;
	
	std::array<DrawIndirectBuffer,CASCADE_MAX_COUNT> m_drawIndirectSSBOShadowDepths {};

	Scene& getActiveScene();

private:
	void allocateSceneTextures(uint32 width,uint32 height,bool forceAllocate = false);
	void meshCollect();
	void pmxCollect(VkCommandBuffer cmd);

private:
	SceneTextures* m_sceneTextures;
	Renderer* m_renderer;
	Ref<SceneManager> m_sceneManager;
	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
};

}