#include "scene_textures.h"
#include "../vk/vk_rhi.h"

namespace engine{

void SceneTextures::allocate(uint32 width,uint32 height)
{
    if( m_init && width == m_cacheSceneWidth && height == m_cacheSceneHeight)
    {
        return;
    }

    VulkanRHI::get()->waitIdle();
    if(m_init)
    {
        // 释放，然后重新申请全局纹理
        release();
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
        VK_FORMAT_R16G16B16A16_SFLOAT
    );

    m_depthStencilTexture = DepthStencilImage::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height
    );

    m_finalColorTexture = RenderTexture::create(
        VulkanRHI::get()->getVulkanDevice(),
        width,height,
        VK_FORMAT_R8G8B8A8_UNORM
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

    m_init = false;
}

}