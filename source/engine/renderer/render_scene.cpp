#include "render_scene.h"
#include "mesh.h"
#include "../scene/components/staticmesh_renderer.h"
#include "frame_data.h"

namespace engine{

RenderScene::RenderScene(Ref<SceneManager> sceneManager,Ref<shaderCompiler::ShaderCompiler> shaderCompiler)
{
	m_sceneTextures = new SceneTextures();
	m_sceneManager = sceneManager;
	m_shaderCompiler = shaderCompiler;
	m_gbufferSSBO = new SceneUploadSSBO();
}

RenderScene::~RenderScene()
{
	delete m_sceneTextures;
	delete m_gbufferSSBO;
}

void RenderScene::initFrame(uint32 width,uint32 height,bool forceAllocateTextures)
{
	// 1. ����������ʵ�SceneTextures
	allocateSceneTextures(width,height,forceAllocateTextures);

	// 2. �ռ������е�����
	bool bMeshPassRebuild = shaderCompiler::g_meshPassShouldRebuild;
	meshCollect(bMeshPassRebuild);
	if(bMeshPassRebuild) // NOTE: ���ﲻ�Ǻܺ�
	{
		shaderCompiler::g_meshPassShouldRebuild = false;
	}

	// 3. ����ssbo
	uploadGbufferSSBO();
}

void RenderScene::uploadGbufferSSBO()
{
	if(m_cacheGBufferObjectSSBOData.size() == 0) return;

	size_t boundSize = sizeof(GPUObjectData) * m_cacheGBufferObjectSSBOData.size();
	m_gbufferSSBO->objectBuffers->map(boundSize);
	m_gbufferSSBO->objectBuffers->copyTo(
		m_cacheGBufferObjectSSBOData.data(),
		boundSize
	);
	m_gbufferSSBO->objectBuffers->unmap();
	
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

	// ����������ȳ�ʼ�����õ�RenderScene
	initFrame(ScreenTextureInitSize,ScreenTextureInitSize);
	m_gbufferSSBO->init();
}

void RenderScene::release()
{
	m_sceneTextures->release();
	m_gbufferSSBO->release();
}

// NOTE: �������ռ������еľ�̬����
// TODO: ��̬�����ռ�����һ�鼴�ɣ�û��Ҫÿ֡���¡�
void RenderScene::meshCollect(bool bRebuildMaterial)
{
	m_cacheGBufferObjectSSBOData.clear();
	m_cacheGBufferObjectSSBOData.resize(0);
	m_cacheStaticMeshRenderMesh.clear();
	m_cacheStaticMeshRenderMesh.resize(0);

	auto& activeScene = m_sceneManager->getActiveScene();
	auto staticMeshComponents = activeScene.getComponents<StaticMeshComponent>();

	for(auto& componentWeakPtr : staticMeshComponents)
	{
		if(auto component = componentWeakPtr.lock()) // NOTE: ��ȡ����Component
		{
			auto renderMesh = component->getRenderMesh(m_shaderCompiler,m_renderer,bRebuildMaterial);
			m_cacheStaticMeshRenderMesh.push_back(renderMesh);

			GPUObjectData data{};
			data.model = renderMesh.modelMatrix;
			for(auto& subMesh : renderMesh.mesh->subMeshes)
			{
				// ������ζ���GBufferPass��Ⱦ
				m_cacheGBufferObjectSSBOData.push_back(data);
			}
		}
	}
}

}
