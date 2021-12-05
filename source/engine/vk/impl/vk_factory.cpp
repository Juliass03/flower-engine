#include "vk_factory.h"

using namespace engine;

VkPipeline engine::VulkanGraphicsPipelineFactory::buildMeshDrawPipeline(VkDevice device, VkRenderPass pass)
{
    vertexInputInfo = vkVertexInputStateCreateInfo();

    vertexInputInfo.pVertexAttributeDescriptions = vertexInputDescription.attributes.data();
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputDescription.attributes.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexInputDescription.bindings.data();
    vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputDescription.bindings.size();

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.stageCount = (uint32_t)shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    std::vector<VkDynamicState> dynamicStates;
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR); 
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicState.pDynamicStates = dynamicStates.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
    pipelineInfo.pDynamicState = &dynamicState;

    VkPipeline newPipeline;
    if(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&newPipeline) != VK_SUCCESS)
    {
        LOG_GRAPHICS_FATAL("Fail to create graphics pipeline!");
        return VK_NULL_HANDLE;
    }
    else
    {
        return newPipeline;
    }
}

VkPipeline engine::VulkanGraphicsPipelineFactory::buildMeshDrawPipeline(VkDevice device,VkRenderPass pass,VkPipelineColorBlendStateCreateInfo cb)
{
    vertexInputInfo = vkVertexInputStateCreateInfo();

    vertexInputInfo.pVertexAttributeDescriptions = vertexInputDescription.attributes.data();
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputDescription.attributes.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexInputDescription.bindings.data();
    vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputDescription.bindings.size();

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending = cb;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.stageCount = (uint32_t)shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    std::vector<VkDynamicState> dynamicStates;
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR); 
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicStates.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);

    dynamicState.pDynamicStates = dynamicStates.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
    pipelineInfo.pDynamicState = &dynamicState;

    VkPipeline newPipeline;
    if(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&newPipeline) != VK_SUCCESS)
    {
        LOG_GRAPHICS_FATAL("Fail to create graphics pipeline!");
        return VK_NULL_HANDLE;
    }
    else
    {
        return newPipeline;
    }
}
