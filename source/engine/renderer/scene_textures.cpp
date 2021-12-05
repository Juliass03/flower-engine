#include "scene_textures.h"
#include "../vk/vk_rhi.h"
#include "../shader_compiler/shader_compiler.h"
#include "renderer.h"
#include "render_scene.h"

namespace engine{

static AutoCVarInt32 cVarShadowDrawDistance(
	"r.Shadow.HardwarePCF",
	"Cascade shadow use hardware pcf.",
	"Shadow",
	0,
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

VkFormat SceneTextures::getDepthStencilFormat()
{
    return VulkanRHI::get()->getVulkanDevice()->findDepthOnlyFormat();
}

VkSampler SceneTextures::getCascadeShadowDepthMapArraySampler()
{
    VkSamplerCreateInfo ci{};

    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.magFilter = VK_FILTER_NEAREST;
    ci.minFilter = VK_FILTER_NEAREST;
    ci.borderColor = reverseZOpen() ? VK_BORDER_COLOR_INT_TRANSPARENT_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    ci.compareEnable = VK_FALSE;
    ci.anisotropyEnable = VK_FALSE;
    ci.minLod = 0.0f;
    ci.maxLod = 0.0f;
    ci.mipLodBias = 0.0f;
    ci.unnormalizedCoordinates = VK_FALSE;

    if(cVarShadowDrawDistance.get() == 1)
    {
        ci.compareEnable = VK_TRUE;
        ci.minFilter = VK_FILTER_LINEAR;
        ci.magFilter = VK_FILTER_LINEAR;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.compareOp = reverseZOpen() ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    }

    return VulkanRHI::get()->createSampler(ci);
}

void SceneTextures::allocate(uint32 width,uint32 height,bool forceAllocate)
{
    if(!forceAllocate && m_init && width == m_cacheSceneWidth && height == m_cacheSceneHeight)
    {
        return;
    }
    
    VulkanRHI::get()->waitIdle();
    
    if(m_init)
    {
        release(); // 释放，然后重新申请全局纹理
        for(auto& callBackPair : m_callbackBeforeSceneTextureRecreate)
        {
            callBackPair.second();
        }
    }

    m_cacheSceneWidth = width;
    m_cacheSceneHeight = height;
        
	m_sceneColorTexture = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getHDRSceneColorFormat(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT
    );

    m_depthStencilTexture = DepthOnlyImage::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height
    );

    m_finalColorTexture = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getLDRSceneColorFormat() 
    );

    m_gbufferBaseColorRoughness = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getGbufferBaseColorRoughnessFormat()
    );

    m_gbufferEmissiveAo = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getGbufferEmissiveAoFormat()
    );

    m_gbufferNormalMetal = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getGbufferNormalMetalFormat(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );

	m_historyTexture = RenderTexture::create(
		VulkanRHI::get()->getVulkanDevice(),
		width,
        height,
		getHistoryFormat(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
	);

    m_velocityTexture = RenderTexture::create(
		VulkanRHI::get()->getVulkanDevice(),
		width,
        height,
		getVelocityFormat(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );

    m_taaTexture = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,
        height,
        getTAAFormat(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );

    m_downsampleChainTexture = RenderTexture::create(
		VulkanRHI::get()->getVulkanDevice(),
		std::max(1u, width   / 2), // 第0级从半分辨率开始
        std::max(1u, height  / 2), // 第0级从半分辨率开始
        g_downsampleCount,
        true,
		getDownSampleFormat(),
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );

    m_downsampleChainTexture->transitionLayoutImmediately(VulkanRHI::get()->getGraphicsCommandPool(),VulkanRHI::get()->getGraphicsQueue(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if(m_needPrepareTexture)
    {
        m_brdflutTexture = RenderTexture::create(
            VulkanRHI::get()->getVulkanDevice(),
            getBRDFLutDim(),
            getBRDFLutDim(),
            getBRDFLutFormat(),
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
        );

        m_irradiancePrefilterTextureCube = RenderTextureCube::create(
            VulkanRHI::get()->getVulkanDevice(),
            getPrefilterCubeDim(),
            getPrefilterCubeDim(),
            1,
            getIrradiancePrefilterCubeFormat(),
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
        );

        m_specularPrefilterTextureCube = RenderTextureCube::create(
			VulkanRHI::get()->getVulkanDevice(),
			getSpecularPrefilterCubeDim(),
            getSpecularPrefilterCubeDim(),
			-1,
			getSpecularPrefilterCubeFormat(),
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT,
            true
        );

#if 0
        m_envTextureCube = RenderTextureCube::create(
            VulkanRHI::get()->getVulkanDevice(),
            getEnvCubeDim(),
            getEnvCubeDim(),
            1,
            getEnvCubeFormat(),
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
        );
#endif
    }

	static int32* singleShadowMapSizePtr = CVarSystem::get()->getInt32CVar("r.Shadow.ShadowMapSize");
    uint32 singleShadowMapWidth  = uint32(*singleShadowMapSizePtr);
    uint32 singleShadowMapHeight = uint32(*singleShadowMapSizePtr);
	uint32 cascadeNum = CASCADE_MAX_COUNT;

    m_cascadeShadowDepthMapArray = DepthOnlyTextureArray::create(
		VulkanRHI::get()->getVulkanDevice(),
        singleShadowMapWidth,
        singleShadowMapHeight,
        cascadeNum
    );

    for(auto& callBackPair : m_callbackAfterSceneTextureRecreate)
    {
        callBackPair.second();
    }

    LOG_GRAPHICS_TRACE("Reallocate scene textures with width: {0}, height: {1}.",width,height);

    m_init = true;
    m_needPrepareTexture = false; // Only prepare once.
}

void SceneTextures::release(bool bAll)
{
    if(bAll)
    {
        CHECK(m_needPrepareTexture == false);

        CHECK(m_brdflutTexture);
        CHECK(m_irradiancePrefilterTextureCube);
        CHECK(m_specularPrefilterTextureCube);

        delete m_brdflutTexture;
        delete m_irradiancePrefilterTextureCube;
        delete m_specularPrefilterTextureCube;
#if 0
        CHECK(m_envTextureCube);
        delete m_envTextureCube;
#endif
    }

    if(!m_init)
    {
        return;
    }

    delete m_depthStencilTexture;
    delete m_sceneColorTexture;
    delete m_finalColorTexture;
    delete m_gbufferBaseColorRoughness;
    delete m_gbufferEmissiveAo;
    delete m_gbufferNormalMetal;
    delete m_cascadeShadowDepthMapArray;
    delete m_historyTexture;
    delete m_velocityTexture;
    delete m_taaTexture;
    delete m_downsampleChainTexture;

    m_init = false;
}

Ref<VulkanImage> SceneTextures::getHistory()
{
    return m_historyTexture;
}

Ref<VulkanImage> SceneTextures::getTAA()
{
   return m_taaTexture;
}

void SceneTextures::frameBegin()
{

}

}