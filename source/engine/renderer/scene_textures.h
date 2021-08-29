#pragma once
#include "../vk/vk_rhi.h"

namespace engine{
constexpr auto ScreenTextureInitSize = 10u;
// 一个Scene中需求的所有Global纹理
class SceneTextures
{
private:
	VulkanImage* m_sceneColorTexture;       // HDR SceneColor Texture. R16G16B16A16 SFLOAT
	VulkanImage* m_depthStencilTexture;     // DepthStencil Texture.
	VulkanImage* m_finalColorTexture;       // LDR SceneColor Texture. R8G8B8A8 

	bool m_init = false;
	uint32 m_cacheSceneWidth = ScreenTextureInitSize;
	uint32 m_cacheSceneHeight = ScreenTextureInitSize;

public:
	SceneTextures() = default;
	~SceneTextures() { release(); }

	void allocate(uint32 width,uint32 height);
	void release();

	Ref<VulkanImage> getHDRSceneColor() { return m_sceneColorTexture; }
	Ref<VulkanImage> getDepthSceneColor() { return m_depthStencilTexture; }
	Ref<VulkanImage> getLDRSceneColor() { return m_finalColorTexture; }

	uint32 getWidth() { return m_cacheSceneWidth; } ;
	uint32 getHeight() { return m_cacheSceneHeight; };
private:
	typedef void (RegisterFuncBeforeSceneTextureRecreate)();
	typedef void (RegisterFuncAfterSceneTextureRecreate)();

	std::unordered_map<std::string,std::function<RegisterFuncBeforeSceneTextureRecreate>> m_callbackBeforeSceneTextureRecreate = {};
	std::unordered_map<std::string,std::function<RegisterFuncAfterSceneTextureRecreate>> m_callbackAfterSceneTextureRecreate = {};

public:
	void addBeforeSceneTextureRebuildCallback(std::string name,const std::function<RegisterFuncBeforeSceneTextureRecreate>& func)
	{
		this->m_callbackBeforeSceneTextureRecreate[name] = func;
	}
	void removeBeforeSceneTextureRebuildCallback(std::string name)
	{
		this->m_callbackBeforeSceneTextureRecreate.erase(name);
	}

	void addAfterSceneTextureRebuildCallback(std::string name,const std::function<RegisterFuncAfterSceneTextureRecreate>& func)
	{
		this->m_callbackAfterSceneTextureRecreate[name] = func;
	}
	void removeAfterSceneTextureRebuildCallback(std::string name)
	{
		this->m_callbackAfterSceneTextureRecreate.erase(name);
	}
};

}