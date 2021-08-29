#pragma once
#include "../../vk/vk_rhi.h"
#include "../render_scene.h"

namespace engine{
class Renderer;
class GraphicsPass
{
public:
	GraphicsPass() = delete;
	GraphicsPass(Ref<Renderer> renderer,Ref<RenderScene> scene,const std::string& name) : m_renderer(renderer), m_renderScene(scene),m_passName(name) { }

	virtual ~GraphicsPass(){ release(); }

	void init();
	void release();
	

	VkRenderPass getRenderpass() { return m_renderpass; }
	virtual void dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex) = 0;
private:
	std::string m_passName;

protected:
	VkRenderPass m_renderpass;
	Ref<Renderer> m_renderer;
	Ref<RenderScene> m_renderScene;
	DeletionQueue m_deletionQueue = {};
	

	// �ڽ������ؽ�ǰ��Ҫ����renderpass��Դ��
	virtual void beforeSceneTextureRecreate() = 0;

	// �ڽ������ؽ�����Ҫ����renderpass��Դ��
	virtual void afterSceneTextureRecreate() = 0;

	virtual void initInner() = 0;
};

}