#include "render_scene.h"
#include "mesh.h"
#include "../scene/components/staticmesh_renderer.h"
#include "frame_data.h"
#include "render_prepare.h"
#include "material.h"

constexpr uint32_t SSBO_BINDING_POS = 0;

namespace engine{

RenderScene::RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler)
{
	m_sceneManager = sceneManager;
	m_shaderCompiler = shaderCompiler;

	m_sceneTextures = new SceneTextures();
	m_meshObjectSSBO = new SceneUploadSSBO<GPUObjectData>();
	m_meshMaterialSSBO = new SceneUploadSSBO<GPUMaterialData>();
	m_drawIndirectSSBOGbuffer = {};
}

RenderScene::~RenderScene()
{
	delete m_sceneTextures;

	delete m_meshObjectSSBO;
	delete m_meshMaterialSSBO;
}

void RenderScene::initFrame(uint32 width,uint32 height,bool forceAllocateTextures)
{
	// 1. 首先申请合适的SceneTextures
	allocateSceneTextures(width,height,forceAllocateTextures);
}

void RenderScene::renderPrepare(const GPUFrameData& view)
{
	// 1. 收集场景中的网格
	meshCollect();

	// 2. 更新ssbo
	uploadMeshSSBO();

	// 3. cpu视锥剔除(已废弃)
	// frustumCulling(m_cacheStaticMeshRenderMesh,view);
}

void RenderScene::uploadMeshSSBO()
{
	if(m_cacheMeshObjectSSBOData.size()   == 0) return;
	if(m_cacheMeshMaterialSSBOData.size() == 0) return;
	{
		size_t objBoundSize = sizeof(GPUObjectData) * m_cacheMeshObjectSSBOData.size();

		m_meshObjectSSBO->buffers->map(objBoundSize);
		m_meshObjectSSBO->buffers->copyTo(
			m_cacheMeshObjectSSBOData.data(),
			objBoundSize
		);
		m_meshObjectSSBO->buffers->unmap();
	}
	{
		size_t materialBoundSize = sizeof(GPUMaterialData) * m_cacheMeshMaterialSSBOData.size();

		m_meshMaterialSSBO->buffers->map(materialBoundSize);
		m_meshMaterialSSBO->buffers->copyTo(
			m_cacheMeshMaterialSSBOData.data(),
			materialBoundSize
		);
		m_meshMaterialSSBO->buffers->unmap();
	}
}

bool RenderScene::isSceneEmpty()
{
	return m_cacheMeshObjectSSBOData.size() == 0;
}

Scene& RenderScene::getActiveScene()
{
	return m_sceneManager->getActiveScene();
}

void RenderScene::allocateSceneTextures(uint32 width,uint32 height,bool forceAllocate)
{
	m_sceneTextures->allocate(width,height,forceAllocate);
}

void RenderScene::init(Renderer* renderer)
{
	m_renderer = renderer;

	// 无论如何首先初始化能用的RenderScene
	initFrame(ScreenTextureInitSize,ScreenTextureInitSize);

	m_meshObjectSSBO->init(SSBO_BINDING_POS);
	m_meshMaterialSSBO->init(SSBO_BINDING_POS);
	m_drawIndirectSSBOGbuffer.init(SSBO_BINDING_POS);
}

void RenderScene::release()
{
	m_sceneTextures->release();

	m_meshObjectSSBO->release();
	m_meshMaterialSSBO->release();
	m_drawIndirectSSBOGbuffer.release();
}

// NOTE: 在这里收集场景中的静态网格
// TODO: 静态网格收集进行一遍即可，没必要每帧更新。
void RenderScene::meshCollect()
{
	m_cacheMeshObjectSSBOData.clear();
	m_cacheMeshObjectSSBOData.resize(0);

	m_cacheMeshMaterialSSBOData.clear();
	m_cacheMeshMaterialSSBOData.resize(0);

	m_cacheStaticMeshRenderMesh.submesh.clear();
	m_cacheStaticMeshRenderMesh.submesh.resize(0);

	m_cacheIndirectCommands.clear();
	m_cacheIndirectCommands.resize(0);

	auto& activeScene = m_sceneManager->getActiveScene();
	auto staticMeshComponents = activeScene.getComponents<StaticMeshComponent>();

	// TODO: Parallel For
	for(auto& componentWeakPtr : staticMeshComponents)
	{
		if(auto component = componentWeakPtr.lock()) // NOTE: 获取网格Component
		{
			std::vector<RenderSubMesh> renderMeshes = component->getRenderMesh(m_renderer);

			// TODO: Parallel For
			for(auto& subMesh : renderMeshes)
			{
				// NOTE: 无论可见与否都创建对应的SSBO
				GPUObjectData objData{};
				objData.model = subMesh.modelMatrix;
				objData.sphereBounds = glm::vec4(subMesh.renderBounds.origin,subMesh.renderBounds.radius);
				objData.extents = glm::vec4(subMesh.renderBounds.extents,1.0f);
				objData.firstInstance = 0;
				objData.vertexOffset = 0; // 我们已经在cpu那里做好了vertexOffset故这里填零
										  
				objData.indexCount = subMesh.indexCount;
				objData.firstIndex = subMesh.indexStartPosition;

				m_cacheMeshObjectSSBOData.push_back(objData);

				// NOTE: 材质添加
				GPUMaterialData matData{};
				matData = subMesh.cacheMaterial->getGPUMaterialData();
				m_cacheMeshMaterialSSBOData.push_back(matData);

				m_cacheStaticMeshRenderMesh.submesh.push_back(subMesh);
			}
		}
	}
}

void RenderScene::DrawIndirectBuffer::init(uint32 bindingPos)
{
	auto bufferSize = sizeof(VkDrawIndexedIndirectCommand) * MAX_SSBO_OBJECTS;
	this->size = bufferSize;

	drawIndirectSSBO = VulkanBuffer::create(
		VulkanRHI::get()->getVulkanDevice(),
		VulkanRHI::get()->getGraphicsCommandPool(),
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr
	);

	VkDescriptorBufferInfo bufInfo = {};
	bufInfo.buffer = *drawIndirectSSBO;
	bufInfo.offset = 0;
	bufInfo.range = bufferSize;

	VulkanRHI::get()->vkDescriptorFactoryBegin()
		.bindBuffer(bindingPos,&bufInfo,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_SHADER_STAGE_COMPUTE_BIT)
		.build(descriptorSets,descriptorSetLayout);

	bFirstInit = true;
}

void RenderScene::DrawIndirectBuffer::release()
{
	delete drawIndirectSSBO;
	bFirstInit = false;
}

}
