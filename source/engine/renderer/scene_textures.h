#pragma once
#include "../vk/vk_rhi.h"

namespace engine{

constexpr auto ScreenTextureInitSize = 10u;

// 一个Scene中需求的所有Global纹理
class SceneTextures
{
private:
	VulkanImage* m_gbufferBaseColorRoughness; // R8G8B8A8 rgb存基础色，a通道存粗糙度
	VulkanImage* m_gbufferNormalMetal;        // r16g16b16a16 rgb存世界空间法线，a通道存金属度
	VulkanImage* m_gbufferEmissiveAo;         // R8G8B8A8 rgb存自发光颜色，a通道存模型AO

	VulkanImage* m_sceneColorTexture;        // HDR SceneColor Texture. R16G16B16A16 SFLOAT
	VulkanImage* m_depthStencilTexture;      // DepthStencil Texture.
	VulkanImage* m_finalColorTexture;        // LDR SceneColor Texture. R8G8B8A8 


	// Cascade shadow texture array.
	DepthOnlyTextureArray* m_cascadeShadowDepthMapArray;

	bool m_init = false;
	uint32 m_cacheSceneWidth = ScreenTextureInitSize;
	uint32 m_cacheSceneHeight = ScreenTextureInitSize;

public:
	SceneTextures() = default;
	~SceneTextures() { release(); }

	static VkFormat getHDRSceneColorFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getDepthStencilFormat();
	static VkFormat getLDRSceneColorFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }
	static VkFormat getGbufferBaseColorRoughnessFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }
	static VkFormat getGbufferNormalMetalFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }
	static VkFormat getGbufferEmissiveAoFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }

	DepthOnlyTextureArray* getCascadeShadowDepthMapArray(){return m_cascadeShadowDepthMapArray;}
	VkSampler getCascadeShadowDepthMapArraySampler();

	void allocate(uint32 width,uint32 height,bool forceAllocate = false);
	void release();

	Ref<VulkanImage> getHDRSceneColor() { return m_sceneColorTexture; }
	Ref<VulkanImage> getDepthStencil() { return m_depthStencilTexture; }
	Ref<VulkanImage> getLDRSceneColor() { return m_finalColorTexture; }
	Ref<VulkanImage> getGbufferBaseColorRoughness() { return m_gbufferBaseColorRoughness; }
	Ref<VulkanImage> getGbufferNormalMetal() { return m_gbufferNormalMetal; }
	Ref<VulkanImage> getGbufferEmissiveAo() { return m_gbufferEmissiveAo; }

	uint32 getWidth() { return m_cacheSceneWidth; };
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