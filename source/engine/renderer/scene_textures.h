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
	// Also use as taa texture in PostProcess;

	VulkanImage* m_gbufferEmissiveAo;         // R8G8B8A8 rgb存自发光颜色，a通道存模型AO

	VulkanImage* m_sceneColorTexture;        // HDR SceneColor Texture. R16G16B16A16 SFLOAT
	VulkanImage* m_depthStencilTexture;      // DepthStencil Texture.
	VulkanImage* m_finalColorTexture;        // LDR SceneColor Texture. R8G8B8A8 

	VulkanImage* m_brdflutTexture; // R16G16

	VulkanImage* m_irradiancePrefilterTextureCube; // R16G16B16A16
	VulkanImage* m_specularPrefilterTextureCube; // R16G16B16A16

	VulkanImage* m_historyTexture;  //  R16G16B16A16
	VulkanImage* m_taaTexture;
	VulkanImage* m_velocityTexture; //  R16G16

	bool bLastFrameHistory = false; // History Texture PingPong

#if 0
	VulkanImage* m_envTextureCube; // R16G16B16A16
#endif

	// Cascade shadow texture array.
	DepthOnlyTextureArray* m_cascadeShadowDepthMapArray;

	bool m_init = false;
	bool m_needPrepareTexture = true;

	uint32 m_cacheSceneWidth = ScreenTextureInitSize;
	uint32 m_cacheSceneHeight = ScreenTextureInitSize;

public:
	SceneTextures() = default;
	~SceneTextures() { release(true); }

	static VkFormat getHDRSceneColorFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getDepthStencilFormat();
	static VkFormat getLDRSceneColorFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }
	static VkFormat getGbufferBaseColorRoughnessFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }
	static VkFormat getGbufferNormalMetalFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getGbufferEmissiveAoFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }

	static VkFormat getIrradiancePrefilterCubeFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getSpecularPrefilterCubeFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getBRDFLutFormat() { return VK_FORMAT_R16G16_SFLOAT; }

	static VkFormat getHistoryFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	static VkFormat getVelocityFormat() { return VK_FORMAT_R16G16_SFLOAT; }
	static VkFormat getTAAFormat() { return getHistoryFormat(); }

	static uint32 getBRDFLutDim() { return 512; }
	static uint32 getPrefilterCubeDim() { return 128; }
	static uint32 getSpecularPrefilterCubeDim() { return 1024; }

	DepthOnlyTextureArray* getCascadeShadowDepthMapArray(){return m_cascadeShadowDepthMapArray;}
	VkSampler getCascadeShadowDepthMapArraySampler();

	void allocate(uint32 width,uint32 height,bool forceAllocate = false);
	void release(bool bAll = false);

	Ref<VulkanImage> getHDRSceneColor() { return m_sceneColorTexture; }
	Ref<VulkanImage> getDepthStencil() { return m_depthStencilTexture; }
	Ref<VulkanImage> getLDRSceneColor() { return m_finalColorTexture; }
	Ref<VulkanImage> getGbufferBaseColorRoughness() { return m_gbufferBaseColorRoughness; }
	Ref<VulkanImage> getGbufferNormalMetal() { return m_gbufferNormalMetal; }
	Ref<VulkanImage> getGbufferEmissiveAo() { return m_gbufferEmissiveAo; }
	Ref<VulkanImage> getBRDFLut() { return m_brdflutTexture; }
	Ref<VulkanImage> getIrradiancePrefilterCube() { return m_irradiancePrefilterTextureCube; }
	Ref<VulkanImage> getSpecularPrefilterCube() { return m_specularPrefilterTextureCube; }

	Ref<VulkanImage> getHistory();
	Ref<VulkanImage> getVelocity() { return m_velocityTexture; }
	Ref<VulkanImage> getTAA();

	bool IsUseSecondTAART() { return bLastFrameHistory; }
#if 0
	Ref<VulkanImage> getEnvCube() { return m_envTextureCube; }
	static uint32 getEnvCubeDim() { return 1024; }
	static VkFormat getEnvCubeFormat() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
#endif

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

	void frameBegin();
};

}