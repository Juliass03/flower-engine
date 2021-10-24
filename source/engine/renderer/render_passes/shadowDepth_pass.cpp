#include "shadowDepth_pass.h"
#include "../../launch/launch_engine_loop.h"
#include "../mesh.h"
#include "../material.h"
#include "../renderer.h"

using namespace engine;

void engine::ShadowDepthPass::initInner()
{
    createRenderpass();
    createFramebuffers();

    m_deletionQueue.push([&]()
    {
        destroyRenderpass();
        destroyFramebuffers();
    });
}

void engine::ShadowDepthPass::beforeSceneTextureRecreate()
{
    destroyFramebuffers();
}

void engine::ShadowDepthPass::afterSceneTextureRecreate()
{
    createFramebuffers();
}

void engine::ShadowDepthPass::dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex)
{
    auto sceneTextureExtent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();
    VkExtent2D sceneTextureExtent2D{};
    sceneTextureExtent2D.width = sceneTextureExtent.width;
    sceneTextureExtent2D.height = sceneTextureExtent.height;

    VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(getRenderpass(),sceneTextureExtent2D,m_framebuffers[backBufferIndex]);
    VkClearValue clearValue{};
    clearValue.depthStencil = { getEngineClearZFar(), 1 };

    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd,&rpInfo,VK_SUBPASS_CONTENTS_INLINE);

    VkRect2D scissor{};
    scissor.extent = sceneTextureExtent2D;
    scissor.offset = {0,0};
    vkCmdSetScissor(cmd,0,1,&scissor);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)sceneTextureExtent2D.width;
    viewport.height = (float)sceneTextureExtent2D.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd,0,1,&viewport);
    vkCmdSetDepthBias(cmd,0,0,0);

    { // TODO:

    }
    vkCmdEndRenderPass(cmd);
}

void engine::ShadowDepthPass::createRenderpass()
{
    VkAttachmentDescription attachmentDesc{};
    attachmentDesc.format = SceneTextures::getDepthStencilFormat();
    attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 0;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachmentDesc;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = (uint32)dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();

    m_renderpass = VulkanRHI::get()->createRenderpass(renderPassInfo);
}

void engine::ShadowDepthPass::destroyRenderpass()
{
    VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void engine::ShadowDepthPass::createFramebuffers()
{
    auto extent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();

    VkExtent2D extent2D{};
    extent2D.width = extent.width;
    extent2D.height = extent.height;

    VulkanFrameBufferFactory fbf {};
    const uint32 swapchain_imagecount = (uint32)VulkanRHI::get()->getSwapchainImages().size();
    m_framebuffers.resize(swapchain_imagecount);

    for(uint32 i = 0; i < swapchain_imagecount; i++)
    {
        fbf.setRenderpass(m_renderpass)
           .addAttachment(m_renderScene->getSceneTextures().getDepthStencil());
        m_framebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
    }
}

void engine::ShadowDepthPass::destroyFramebuffers()
{
    for(uint32 i = 0; i < m_framebuffers.size(); i++)
    {
        VulkanRHI::get()->destroyFramebuffer(m_framebuffers[i]);
    }

    m_framebuffers.resize(0);
}
