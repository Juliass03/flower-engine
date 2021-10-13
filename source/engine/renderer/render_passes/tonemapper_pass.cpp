#include "tonemapper_pass.h"
#include "../../asset_system/asset_system.h"
#include "../renderer.h"
using namespace engine;

void engine::TonemapperPass::initInner()
{
    createRenderpass();
    createFramebuffers();
    bInitPipeline = false;
    createPipeline();
    m_deletionQueue.push([&]()
    {
        destroyRenderpass();
        destroyFramebuffers();
        destroyPipeline();
     });
}

void engine::TonemapperPass::beforeSceneTextureRecreate()
{
    destroyFramebuffers();
    destroyPipeline();
}

void engine::TonemapperPass::afterSceneTextureRecreate()
{
    createFramebuffers();
    createPipeline();
}

void engine::TonemapperPass::dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex)
{
    auto sceneTextureExtent = m_renderScene->getSceneTextures().getLDRSceneColor()->getExtent();
    VkExtent2D sceneTextureExtent2D{};
    sceneTextureExtent2D.width = sceneTextureExtent.width;
    sceneTextureExtent2D.height = sceneTextureExtent.height;

    VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(
        getRenderpass(),
        sceneTextureExtent2D,
        m_framebuffers[backBufferIndex]
    );

    VkClearValue colorValue;
    colorValue.color = {{ 0.0f, 0.0f, 0.0f, 0.0f }};

    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;
    depthClear.depthStencil.stencil = 1;

    VkClearValue clearValues[2] = {colorValue, depthClear};
    rpInfo.clearValueCount = 2;
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

    vkCmdBindDescriptorSets(
        cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayouts[backBufferIndex],
        0, // PassSet #0
        1,
        &m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set,0,nullptr
    );

    vkCmdBindDescriptorSets(
        cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayouts[backBufferIndex],
        1, // PassSet #1
        1,
        &m_renderer->getFrameData().m_viewDataDescriptorSets[backBufferIndex].set,0,nullptr
    );

    vkCmdBindDescriptorSets(
        cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayouts[backBufferIndex],
        2, // PassSet #2
        1,
        &m_tonemapperPassDescriptorSets[backBufferIndex].set,0,nullptr
    );
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_pipelines[backBufferIndex]);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void engine::TonemapperPass::createRenderpass()
{
    VkAttachmentDescription colorAttachment = {};

    // NOTE: Tonemapper‰÷»æµΩLDRÕº…œ
    colorAttachment.format =  SceneTextures::getLDRSceneColorFormat();

    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = nullptr;

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

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &colorAttachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies.data();

    m_renderpass = VulkanRHI::get()->createRenderpass(render_pass_info);
}

void engine::TonemapperPass::destroyRenderpass()
{
    VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void engine::TonemapperPass::createFramebuffers()
{
    auto extent = m_renderScene->getSceneTextures().getLDRSceneColor()->getExtent();

    VkExtent2D extent2D{};
    extent2D.width = extent.width;
    extent2D.height = extent.height;

    VulkanFrameBufferFactory fbf {};
    const uint32 swapchain_imagecount = (uint32)VulkanRHI::get()->getSwapchainImages().size();
    m_framebuffers.resize(swapchain_imagecount);

    for(uint32 i = 0; i < swapchain_imagecount; i++)
    {
        fbf.setRenderpass(m_renderpass)
            .addAttachment(
                m_renderScene->getSceneTextures().getLDRSceneColor()->getImageView(),
                extent2D
            );
        m_framebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
    }
}

void engine::TonemapperPass::destroyFramebuffers()
{
    for(uint32 i = 0; i < m_framebuffers.size(); i++)
    {
        VulkanRHI::get()->destroyFramebuffer(m_framebuffers[i]);
    }

    m_framebuffers.resize(0);
}

void engine::TonemapperPass::createPipeline()
{
    if(bInitPipeline) return;

    // Create descriptor sets first.
    uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();
    m_tonemapperPassDescriptorSets.resize(backBufferCount);
    m_tonemapperPassDescriptorSetLayouts.resize(backBufferCount);
    for(uint32 index = 0; index < backBufferCount; index++)
    {
        VkDescriptorImageInfo sceneColorImage = {};
        sceneColorImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneColorImage.imageView = m_renderScene->getSceneTextures().getHDRSceneColor()->getImageView();
        sceneColorImage.sampler = VulkanRHI::get()->getPointClampSampler();

        m_renderer->vkDynamicDescriptorFactoryBegin(index)
            .bindImage(0,&sceneColorImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_tonemapperPassDescriptorSets[index],m_tonemapperPassDescriptorSetLayouts[index]);
    }

    // Then we can create pipeline and pipelinelayout.
    m_pipelines.resize(backBufferCount);
    m_pipelineLayouts.resize(backBufferCount);
    for(uint32 index = 0; index < backBufferCount; index++)
    {
        VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

        // Current we don't need push const.
        plci.pushConstantRangeCount = 0;
        plci.pPushConstantRanges = nullptr;

        std::vector<VkDescriptorSetLayout> setLayouts = {
            m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout,
            m_renderer->getFrameData().m_viewDataDescriptorSetLayouts[index].layout,
            m_tonemapperPassDescriptorSetLayouts[index].layout 
        };

        plci.setLayoutCount = (uint32)setLayouts.size();
        plci.pSetLayouts = setLayouts.data();

        m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

        VulkanGraphicsPipelineFactory gpf = {};

        auto packShader = m_shaderCompiler->getShader(s_shader_tonemapper,shaderCompiler::EShaderPass::Tonemapper);
        auto vertShader = packShader.vertex;
        auto fragShader = packShader.frag;

        gpf.shaderStages.clear();
        gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertShader));
        gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragShader));
        gpf.vertexInputInfo = vkVertexInputStateCreateInfo();

        gpf.vertexInputInfo.vertexBindingDescriptionCount = 0;
        gpf.vertexInputInfo.pVertexBindingDescriptions = nullptr;
        gpf.vertexInputInfo.vertexAttributeDescriptionCount = 0;
        gpf.vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        gpf.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        gpf.depthStencil = vkDepthStencilCreateInfo(false, false, VK_COMPARE_OP_ALWAYS);

        auto sceneTextureExtent = m_renderScene->getSceneTextures().getLDRSceneColor()->getExtent();
        VkExtent2D sceneTextureExtent2D{};
        sceneTextureExtent2D.width = sceneTextureExtent.width;
        sceneTextureExtent2D.height = sceneTextureExtent.height;

        gpf.viewport.x = 0.0f;
        gpf.viewport.y = 0.0f;
        gpf.viewport.width =  (float)sceneTextureExtent2D.width;
        gpf.viewport.height = (float)sceneTextureExtent2D.height;
        gpf.viewport.minDepth = 0.0f;
        gpf.viewport.maxDepth = 1.0f;
        gpf.scissor.offset = { 0, 0 };
        gpf.scissor.extent = sceneTextureExtent2D;

        gpf.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
        gpf.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        gpf.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        gpf.multisampling = vkMultisamplingStateCreateInfo();
        gpf.colorBlendAttachments = { vkColorBlendAttachmentState()};

        gpf.pipelineLayout = m_pipelineLayouts[index];
        m_pipelines[index] = gpf.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),m_renderpass);
    }

    bInitPipeline = true;
}

void engine::TonemapperPass::destroyPipeline()
{
    if(!bInitPipeline) return;

    for(uint32 index = 0; index < m_pipelines.size(); index++)
    {
        vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
        vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
    }
    m_pipelines.resize(0);
    m_pipelineLayouts.resize(0);

    bInitPipeline = false;
}
