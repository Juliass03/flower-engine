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
        width,height,
        getHDRSceneColorFormat()
    );

    m_depthStencilTexture = DepthOnlyImage::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height
    );

    m_finalColorTexture = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height,
        getLDRSceneColorFormat() 
    );

    m_gbufferBaseColorRoughness = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height,
        getGbufferBaseColorRoughnessFormat()
    );

    m_gbufferEmissiveAo = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height,
        getGbufferEmissiveAoFormat()
    );

    m_gbufferNormalMetal = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height,
        getGbufferNormalMetalFormat()
    );

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
}

void SceneTextures::release()
{
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

    m_init = false;
}

}