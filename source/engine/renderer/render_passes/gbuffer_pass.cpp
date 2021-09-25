#include "gbuffer_pass.h"
#include "../../launch/launch_engine_loop.h"
#include "../mesh.h"
#include "../material.h"
#include "../renderer.h"

using namespace engine;

void engine::GBufferPass::initInner()
{
    createRenderpass();
    createFramebuffers();

    m_deletionQueue.push([&]()
    {
        destroyRenderpass();
        destroyFramebuffers();
    });
}

void engine::GBufferPass::beforeSceneTextureRecreate()
{
    destroyFramebuffers();
}

void engine::GBufferPass::afterSceneTextureRecreate()
{
    createFramebuffers();
}

void engine::GBufferPass::dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex)
{
    auto sceneTextureExtent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();
    VkExtent2D sceneTextureExtent2D{};
    sceneTextureExtent2D.width = sceneTextureExtent.width;
    sceneTextureExtent2D.height = sceneTextureExtent.height;

    VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(getRenderpass(),sceneTextureExtent2D,m_framebuffers[backBufferIndex]);
    std::array<VkClearValue,5> clearValues{};

    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[4].depthStencil = { 1.0f, 1 };

    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = &clearValues[0];

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

    uint32 drawIndex = 0;
    auto& cacheStaticMesh = m_renderScene->m_cacheStaticMeshRenderMesh;
	VkPipelineLayout activeLayout = VK_NULL_HANDLE;
	VkPipeline activePipeline = VK_NULL_HANDLE;
    for (auto& renderMesh : cacheStaticMesh)
    {
        renderMesh.mesh->vertexBuffer->bind(cmd);

        for(auto& subMesh : renderMesh.mesh->subMeshes)
        {
            Material* activeMaterial = nullptr;
            if(subMesh.material && subMesh.material->gbufferReference)
            {
                activeMaterial = subMesh.material->gbufferReference;
            }
            else
            {
                activeMaterial = m_renderer->getMaterialLibrary()->getFallbackGbuffer();
            }

            const auto& loopLayout = activeMaterial->getShaderInfo()->effect->builtLayout;
            const auto& loopPipeline = activeMaterial->getPipeline(backBufferIndex);

            // 检查是否需要切换pipeline
            if(activePipeline != loopPipeline)
            {
                activePipeline = loopPipeline;
                activeLayout = loopLayout;

                auto& activeFrameData = m_renderer->getFrameData();

                vkCmdBindDescriptorSets(
                    cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                    activeLayout,
                    0, // PassSet #0
                    1,
                    &activeFrameData.m_frameDataDescriptorSets[backBufferIndex].set,0,nullptr
                );

                vkCmdBindDescriptorSets(
                    cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                    activeLayout,
                    1, // PassSet #1
                    1,
                    &activeFrameData.m_viewDataDescriptorSets[backBufferIndex].set,0,nullptr
                );

                // PassSet #2
                m_renderScene->m_gbufferSSBO->bindDescriptorSet(cmd,activeLayout);

                vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,activePipeline);
            }

            // PassSet #3
            activeMaterial->bind(backBufferIndex);

            subMesh.indexBuffer->bind(cmd);
            vkCmdDrawIndexed(cmd, subMesh.indexCount,1,0,0,drawIndex);
            drawIndex ++;
        }
    }

    vkCmdEndRenderPass(cmd);
}

void engine::GBufferPass::createRenderpass()
{
    // TODO: 当前的GBuffer数目为 1
    const size_t gbufferCount = 5;

    std::array<VkAttachmentDescription,gbufferCount> attachmentDescs{};
    for(size_t i = 0; i < attachmentDescs.size(); i++)
    {
        attachmentDescs[i] = {};
        attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
        
        // GBufferPass加载纹理时清理掉它
        attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    attachmentDescs[0].format = m_renderScene->getSceneTextures().getHDRSceneColor()->getFormat();
    attachmentDescs[1].format = m_renderScene->getSceneTextures().getGbufferBaseColorRoughness()->getFormat();
    attachmentDescs[2].format = m_renderScene->getSceneTextures().getGbufferNormalMetal()->getFormat();
    attachmentDescs[3].format = m_renderScene->getSceneTextures().getGbufferEmissiveAo()->getFormat();
    attachmentDescs[4].format = m_renderScene->getSceneTextures().getDepthSceneColor()->getFormat();

    // TODO: 修改为Frame
    attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachmentDescs[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachmentDescs[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachmentDescs[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 4;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pColorAttachments = colorReferences.data();
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.pAttachments = attachmentDescs.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    m_renderpass = VulkanRHI::get()->createRenderpass(renderPassInfo);
}

void engine::GBufferPass::destroyRenderpass()
{
    VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void engine::GBufferPass::createFramebuffers()
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
            .addAttachment(m_renderScene->getSceneTextures().getHDRSceneColor())
            .addAttachment(m_renderScene->getSceneTextures().getGbufferBaseColorRoughness())
            .addAttachment(m_renderScene->getSceneTextures().getGbufferNormalMetal())
            .addAttachment(m_renderScene->getSceneTextures().getGbufferEmissiveAo())
            .addAttachment(m_renderScene->getSceneTextures().getDepthSceneColor());
        m_framebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
    }
}

void engine::GBufferPass::destroyFramebuffers()
{
    for(uint32 i = 0; i < m_framebuffers.size(); i++)
    {
        VulkanRHI::get()->destroyFramebuffer(m_framebuffers[i]);
    }

    m_framebuffers.resize(0);
}
