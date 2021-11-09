#include "renderer.h"
#include "../engine.h"
#include "../vk/vk_rhi.h"
#include "../../imgui/imgui.h"
#include "render_passes/gbuffer_pass.h"
#include "render_passes/tonemapper_pass.h"
#include "material.h"
#include "../scene/components/sceneview_camera.h"
#include "../scene/components/directionalLight.h"
#include "render_passes/lighting_pass.h"
#include "render_passes/cascade_shadowdepth_pass.h"
#include "compute_passes/depth_evaluate_minmax.h"
#include "compute_passes/cascade_setup.h"
#include "render_prepare.h"
#include "mesh.h"
#include "compute_passes/gpu_culling.h"
#include "frustum.h"

using namespace engine;

static AutoCVarInt32 cVarReverseZ(
	"r.Shading.ReverseZ",
	"Enable reverse z. 0 is off, others are on.",
	"Shading",
	1,
	CVarFlags::ReadAndWrite
);

bool engine::reverseZOpen()
{
    return cVarReverseZ.get() > 0;
}

float engine::getEngineClearZFar()
{
    return engine::reverseZOpen() ? 0.0f : 1.0f;
}

float engine::getEngineClearZNear()
{
    return engine::reverseZOpen() ? 1.0f : 0.0f;
}

VkCompareOp engine::getEngineZTestFunc()
{
    return reverseZOpen() ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
}


Renderer::Renderer(Ref<ModuleManager> in) : IRuntimeModule(in)
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	Ref<SceneManager> sceneManager = m_moduleManager->getRuntimeModule<SceneManager>();
	m_renderScene = new RenderScene(sceneManager,shader_compiler);
	m_uiPass = new ImguiPass();
}

bool Renderer::init()
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	CHECK(shader_compiler);

	m_dynamicDescriptorAllocator.resize(VulkanRHI::get()->getSwapchainImageViews().size());
	for(size_t i = 0; i<m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i] = new VulkanDescriptorAllocator();
		m_dynamicDescriptorAllocator[i]->init(VulkanRHI::get()->getVulkanDevice());
	}

	m_renderScene->init(this);
	m_uiPass->initImgui();

	

	// ע��RenderPasses
	// m_renderpasses.push_back(new ShadowDepthPass(this,m_renderScene,shader_compiler,"ShadowDepth"));

	m_gbufferCullingPass = new GpuCullingPass(this,m_renderScene,shader_compiler, "GbufferCulling");

	m_cascasdeCullingPasses = new GpuCullingPass(this,m_renderScene,shader_compiler, "CascadeCulling");
	m_shadowdepthPasses = new ShadowDepthPass(this,m_renderScene,shader_compiler, "CascadeDepth");

	m_gbufferPass = new GBufferPass(this,m_renderScene,shader_compiler,"GBuffer");
	m_depthEvaluateMinMaxPass = new GpuDepthEvaluateMinMaxPass(this,m_renderScene,shader_compiler, "DepthEvaluateMinMax");
	m_cascadeSetupPass = new GpuCascadeSetupPass(this,m_renderScene,shader_compiler, "CascadeSetup");

	m_lightingPass = new LightingPass(this,m_renderScene,shader_compiler,"Lighting");
	m_tonemapperPass = new TonemapperPass(this,m_renderScene,shader_compiler,"ToneMapper");

	// ���ȵ���PerframeData�ĳ�ʼ������ȷ��ȫ�ֵ�PerframeData��ȷ��ʼ��
	m_frameData.init();
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// ����������ÿһ��Renderpass�ĳ�ʼ������
	m_gbufferCullingPass->init();
	m_gbufferPass->init();
	m_depthEvaluateMinMaxPass->init();
	m_cascadeSetupPass->init();

	m_cascasdeCullingPasses->init();
	m_shadowdepthPasses->init();

	m_lightingPass->init();
	m_tonemapperPass->init();

	return true;
}

void Renderer::tick(float dt)
{
	Ref<shaderCompiler::ShaderCompiler> shader_compiler = m_moduleManager->getRuntimeModule<shaderCompiler::ShaderCompiler>();
	CHECK(shader_compiler);

	uint32 backBufferIndex = VulkanRHI::get()->acquireNextPresentImage();
	
	bool bForceAllocate = false;
	if(shaderCompiler::g_shaderPassChange)
	{
		bForceAllocate = true;
		shaderCompiler::g_shaderPassChange = false;
	}

	if( bForceAllocate ||
	   m_screenViewportWidth  != m_renderScene->getSceneTextures().getWidth() ||
	   m_screenViewportHeight != m_renderScene->getSceneTextures().getHeight())
	{
		// NOTE: �����仯��Ҫ���ö�̬�������ز����´���ȫ�ֵ�������
		m_dynamicDescriptorAllocator[backBufferIndex]->resetPools();
		m_frameData.markPerframeDescriptorSetsDirty();
	}

	// NOTE: ͨ��initFrame���뵽���ʵ�RT����frameData�й�����������
	m_renderScene->initFrame(m_screenViewportWidth, m_screenViewportHeight,bForceAllocate);
	m_frameData.buildPerFrameDataDescriptorSets(this);

	// ����ȫ�ֵ�frameData
	updateGPUData(dt);

	m_frameData.updateFrameData(m_gpuFrameData);
	m_renderScene->renderPrepare(m_gpuFrameData);


	
	m_gbufferCullingPass->gbuffer_record(backBufferIndex); // gbuffer culling

	m_gbufferPass->dynamicRecord(backBufferIndex); // gbuffer rendering

	m_depthEvaluateMinMaxPass->record(backBufferIndex); // depth evaluate min max
	m_cascadeSetupPass->record(backBufferIndex);// cascade setup.

	m_cascasdeCullingPasses->cascade_record(backBufferIndex); // cascade culling

	m_shadowdepthPasses->dynamicRecord(backBufferIndex); // shadow rendering

	m_lightingPass->dynamicRecord(backBufferIndex); // lighting pass

	m_tonemapperPass->dynamicRecord(backBufferIndex); // tonemapper pass


	uiRecord(backBufferIndex);
	m_uiPass->renderFrame(backBufferIndex);

// TODO: ʹ�� rendergraph ���ⲿ��
// �ύ����
	auto frameStartSemaphore = VulkanRHI::get()->getCurrentFrameWaitSemaphoreRef();
	auto frameEndSemaphore = VulkanRHI::get()->getCurrentFrameFinishSemaphore();

	const bool bSceneEmpty = m_renderScene->isSceneEmpty();

	std::vector<VkPipelineStageFlags> graphicsWaitFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	std::vector<VkPipelineStageFlags> computeWaitFlags = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

	std::vector<VkSemaphore> waitSemaphores = {frameStartSemaphore};



	// 1. gbuffer culling
	VulkanSubmitInfo gbufferCullingSubmitInfo{};
	auto gbufferCullingSemaphore = m_gbufferCullingPass->getSemaphore(backBufferIndex);
	VkCommandBuffer gbufferCullingBuf = m_gbufferCullingPass->getCommandBuf(backBufferIndex)->getInstance();
	gbufferCullingSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(waitSemaphores.data(),(uint32)waitSemaphores.size()) // �ȴ���һ֡���
		.setSignalSemaphore(&gbufferCullingSemaphore,1)
		.setCommandBuffer(&gbufferCullingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&gbufferCullingSubmitInfo.get(),nullptr));

	// 2. gbuffer rendering
	VulkanSubmitInfo gbufferRenderingSubmitInfo{};
	auto gbufferRenderingSemaphore = m_gbufferPass->getSemaphore(backBufferIndex);
	VkCommandBuffer gbufferRenderingBuf = m_gbufferPass->getCommandBuf(backBufferIndex)->getInstance();
	gbufferRenderingSubmitInfo.setWaitStage(computeWaitFlags)// �ȴ��޳����
		.setWaitSemaphore(&gbufferCullingSemaphore,1) 
		.setSignalSemaphore(&gbufferRenderingSemaphore,1) 
		.setCommandBuffer(&gbufferRenderingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&gbufferRenderingSubmitInfo.get(),nullptr));
	
	// 3. depth evaluate min max
	VulkanSubmitInfo depthEvaluateMinMaxSubmitInfo{};
	auto depthEvaluateMinMaxSemaphore = m_depthEvaluateMinMaxPass->getSemaphore(backBufferIndex);
	VkCommandBuffer depthEvaluateMinMaxBuf = m_depthEvaluateMinMaxPass->getCommandBuf(backBufferIndex)->getInstance();
	depthEvaluateMinMaxSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&gbufferRenderingSemaphore,1) 
		.setSignalSemaphore(&depthEvaluateMinMaxSemaphore,1) 
		.setCommandBuffer(&depthEvaluateMinMaxBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&depthEvaluateMinMaxSubmitInfo.get(),nullptr));

	// 4. cascade setup
	VulkanSubmitInfo cascasdeSetupSubmitInfo{};
	auto cascasdeSetupSemaphore = m_cascadeSetupPass->getSemaphore(backBufferIndex);
	VkCommandBuffer cascasdeSetupBuf = m_cascadeSetupPass->getCommandBuf(backBufferIndex)->getInstance();
	cascasdeSetupSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&depthEvaluateMinMaxSemaphore,1)
		.setSignalSemaphore(&cascasdeSetupSemaphore,1) 
		.setCommandBuffer(&cascasdeSetupBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&cascasdeSetupSubmitInfo.get(),nullptr));

	// 5. cascade culling
	VulkanSubmitInfo cascasdeCullingSubmitInfo{};
	auto cascasdeCullingSemaphore = m_cascasdeCullingPasses->getSemaphore(backBufferIndex);
	VkCommandBuffer cascasdeCullingBuf = m_cascasdeCullingPasses->getCommandBuf(backBufferIndex)->getInstance();
	cascasdeCullingSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&cascasdeSetupSemaphore,1)
		.setSignalSemaphore(&cascasdeCullingSemaphore,1) 
		.setCommandBuffer(&cascasdeCullingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&cascasdeCullingSubmitInfo.get(),nullptr));

	// 6. shadow rendering
	VulkanSubmitInfo shadowDepthSubmitInfo{};
	auto shadowDepthSemaphore = m_shadowdepthPasses->getSemaphore(backBufferIndex);
	VkCommandBuffer shadowDepthBuf = m_shadowdepthPasses->getCommandBuf(backBufferIndex)->getInstance();
	shadowDepthSubmitInfo.setWaitStage(computeWaitFlags)
		.setWaitSemaphore(&cascasdeCullingSemaphore,1)
		.setSignalSemaphore(&shadowDepthSemaphore,1) 
		.setCommandBuffer(&shadowDepthBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&shadowDepthSubmitInfo.get(),nullptr));

	// 7. lighting
	VulkanSubmitInfo lightingSubmitInfo{};
	auto lightingSemaphore = m_lightingPass->getSemaphore(backBufferIndex);
	VkCommandBuffer lightingBuf = m_lightingPass->getCommandBuf(backBufferIndex)->getInstance();
	lightingSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&shadowDepthSemaphore,1)
		.setSignalSemaphore(&lightingSemaphore,1) 
		.setCommandBuffer(&lightingBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&lightingSubmitInfo.get(),nullptr));

	// 8. tonemapping
	VulkanSubmitInfo tonemapperSubmitInfo{};
	auto tonemapperSemaphore = m_tonemapperPass->getSemaphore(backBufferIndex);
	VkCommandBuffer tonemapperBuf = m_tonemapperPass->getCommandBuf(backBufferIndex)->getInstance();
	tonemapperSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&lightingSemaphore,1)
		.setSignalSemaphore(&tonemapperSemaphore,1) 
		.setCommandBuffer(&tonemapperBuf,1);
	vkCheck(vkQueueSubmit(VulkanRHI::get()->getGraphicsQueue(),1,&tonemapperSubmitInfo.get(),nullptr));

	// 9. imgui
	VulkanSubmitInfo imguiPassSubmitInfo{};
	VkCommandBuffer cmd_uiPass = m_uiPass->getCommandBuffer(backBufferIndex);
	imguiPassSubmitInfo.setWaitStage(graphicsWaitFlags)
		.setWaitSemaphore(&tonemapperSemaphore,1)
		.setSignalSemaphore(frameEndSemaphore,1)
		.setCommandBuffer(&cmd_uiPass,1);

	std::vector<VkSubmitInfo> submitInfos = { imguiPassSubmitInfo };

	VulkanRHI::get()->resetFence();
	VulkanRHI::get()->submit((uint32_t)submitInfos.size(),submitInfos.data());
	m_uiPass->updateAfterSubmit();

	VulkanRHI::get()->present();
}

void Renderer::release()
{
	m_cascasdeCullingPasses->release(); delete m_cascasdeCullingPasses;
	m_shadowdepthPasses->release(); delete m_shadowdepthPasses;

	m_gbufferPass->release(); delete m_gbufferPass;
	m_depthEvaluateMinMaxPass->release(); delete m_depthEvaluateMinMaxPass;
	m_cascadeSetupPass->release(); delete m_cascadeSetupPass;

	m_lightingPass->release(); delete m_lightingPass;
	m_tonemapperPass->release(); delete m_tonemapperPass;
	m_gbufferCullingPass->release(); delete m_gbufferCullingPass;

	m_uiPass->release(); delete m_uiPass;

	m_renderScene->release(); delete m_renderScene;
	m_frameData.release();

	for(size_t i = 0; i < m_dynamicDescriptorAllocator.size(); i++)
	{
		m_dynamicDescriptorAllocator[i]->cleanup();
		delete m_dynamicDescriptorAllocator[i];
	}
}

void Renderer::UpdateScreenSize(uint32 width,uint32 height)
{
	const uint32 renderSceneWidth = glm::max(width,10u);
	const uint32 renderSceneHeight = glm::max(height,10u);
	m_screenViewportWidth = renderSceneWidth;
	m_screenViewportHeight = renderSceneHeight;
}

void Renderer::uiRecord(size_t i)
{
	m_uiPass->newFrame();

	for(auto& callBackPair : m_uiFunctions)
	{
		callBackPair.second(i);
	}
}

void Renderer::updateGPUData(float dt)
{
	float gametime = g_timer.gameTime();
	m_gpuFrameData.appTime = glm::vec4(
		g_timer.globalPassTime(),
		gametime,
		glm::sin(gametime),
		glm::cos(gametime)
	);

	auto directionalLights = m_renderScene->getActiveScene().getComponents<DirectionalLight>();
	bool bHasValidateDirectionalLight = false;

	if(directionalLights.size()>0)
	{
		for(auto& light:directionalLights)
		{
			if(const auto lightPtr = light.lock())
			{
				bHasValidateDirectionalLight = true;

				m_gpuFrameData.sunLightColor = lightPtr->getColor();
				m_gpuFrameData.sunLightDir = lightPtr->getDirection();

				break;// NOTE: ��ǰ���ǽ������һյ��Ч��ֱ��� 
			}
		}
	}

	if(!bHasValidateDirectionalLight)
	{
		m_gpuFrameData.sunLightColor = glm::vec4(1.0f,1.0f,1.0f,1.0f);
		m_gpuFrameData.sunLightDir = glm::vec4(0.0f,0.34f,0.33f,1.0f);
	}
	

	auto sceneViewCameraNode = m_renderScene->getActiveScene().getSceneViewCameraNode();
	auto sceneViewCameraComponent = sceneViewCameraNode->getComponent<SceneViewCamera>();

	// ���³������
	sceneViewCameraComponent->tick(dt,m_screenViewportWidth,m_screenViewportHeight);

	m_gpuFrameData.camView = sceneViewCameraComponent->getView();
	m_gpuFrameData.camProj = sceneViewCameraComponent->getProjection();
	m_gpuFrameData.camViewProj = m_gpuFrameData.camProj * m_gpuFrameData.camView;
	m_gpuFrameData.camInvertViewProj = glm::inverse(m_gpuFrameData.camViewProj);

	m_gpuFrameData.camWorldPos = glm::vec4(sceneViewCameraComponent->getPosition(),1.0f);

	m_gpuFrameData.cameraInfo = glm::vec4(
		sceneViewCameraComponent->getFoVy(),
		float(m_screenViewportWidth) / float(m_screenViewportHeight),
		sceneViewCameraComponent->getZNear(),
		sceneViewCameraComponent->getZFar()
	);

	Frustum camFrusum{};
	camFrusum.update(m_gpuFrameData.camViewProj);

	// ����ViewFrustum��Ϣ
	m_gpuFrameData.camFrustumPlanes[0] = camFrusum.planes[0];
	m_gpuFrameData.camFrustumPlanes[1] = camFrusum.planes[1];
	m_gpuFrameData.camFrustumPlanes[2] = camFrusum.planes[2];
	m_gpuFrameData.camFrustumPlanes[3] = camFrusum.planes[3];
	m_gpuFrameData.camFrustumPlanes[4] = camFrusum.planes[4];
	m_gpuFrameData.camFrustumPlanes[5] = camFrusum.planes[5];
}


VulkanDescriptorAllocator& Renderer::getDynamicDescriptorAllocator(uint32 i)
{ 
	return *m_dynamicDescriptorAllocator[i]; 
}

VulkanDescriptorFactory Renderer::vkDynamicDescriptorFactoryBegin(uint32 i)
{ 
	return VulkanDescriptorFactory::begin(&VulkanRHI::get()->getDescriptorLayoutCache(), m_dynamicDescriptorAllocator[i]); 
}
