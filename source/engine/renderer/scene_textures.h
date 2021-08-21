#pragma once
#include "../vk/vk_rhi.h"

namespace engine{

// 一个Scene中需求的所有Global纹理
class SceneTextures
{
private:
	VulkanImage* m_sceneColorTexture;       // HDR SceneColor Texture. R16G16B16A16 SFLOAT
	VulkanImage* m_depthStencilTexture; // DepthStencil Texture.

	bool m_init = false;
	uint32 m_cacheSceneWidth = 0;
	uint32 m_cacheSceneHeight = 0;

public:
	SceneTextures() = default;
	~SceneTextures() { release(); }

	void allocate(uint32 width,uint32 height);
	void release();

	Ref<VulkanImage> getSceneColor() { return m_sceneColorTexture; }
	Ref<VulkanImage> getDepthSceneColor() { return m_depthStencilTexture; }

};

}