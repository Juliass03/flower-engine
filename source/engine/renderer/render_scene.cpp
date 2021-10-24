#include "render_scene.h"
#include "mesh.h"
#include "../scene/components/staticmesh_renderer.h"
#include "frame_data.h"
#include "render_prepare.h"
#include "material.h"

namespace engine{

RenderScene::RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler)
{
	m_sceneManager = sceneManager;
	m_shaderCompiler = shaderCompiler;

	m_sceneTextures = new SceneTextures();
	m_meshObjectSSBO = new SceneUploadSSBO<GPUObjectData>();
	m_meshMaterialSSBO = new SceneUploadSSBO<GPUMaterialData>();
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

	// 3. 视锥剔除
	frustumCulling(m_cacheStaticMeshRenderMesh,view);
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

	m_meshObjectSSBO->init();
	m_meshMaterialSSBO->init();
}

void RenderScene::release()
{
	m_sceneTextures->release();

	m_meshObjectSSBO->release();
	m_meshMaterialSSBO->release();
}

// NOTE: 在这里收集场景中的静态网格
// TODO: 静态网格收集进行一遍即可，没必要每帧更新。
// TODO: 合并静态网格
void RenderScene::meshCollect()
{
	m_cacheMeshObjectSSBOData.clear();
	m_cacheMeshObjectSSBOData.resize(0);

	m_cacheMeshMaterialSSBOData.clear();
	m_cacheMeshMaterialSSBOData.resize(0);

	m_cacheStaticMeshRenderMesh.clear();
	m_cacheStaticMeshRenderMesh.resize(0);

	auto& activeScene = m_sceneManager->getActiveScene();
	auto staticMeshComponents = activeScene.getComponents<StaticMeshComponent>();

	for(auto& componentWeakPtr : staticMeshComponents)
	{
		if(auto component = componentWeakPtr.lock()) // NOTE: 获取网格Component
		{
			auto renderMesh = component->getRenderMesh(m_shaderCompiler,m_renderer);
			renderMesh.bMeshVisible = true;
			m_cacheStaticMeshRenderMesh.push_back(renderMesh);

			for(auto& subMesh : renderMesh.submesh)
			{
				// NOTE: 无论可见与否都创建对应的SSBO
				GPUObjectData objData{};
				objData.model = renderMesh.modelMatrix;
				m_cacheMeshObjectSSBOData.push_back(objData);

				// NOTE: 材质添加
				GPUMaterialData matData{};
				matData = subMesh.cacheMaterial->getGPUMaterialData();
				
				m_cacheMeshMaterialSSBOData.push_back(matData);
			}
		}
	}
}

}
