#pragma once
#include "vk_common.h"
#include "vk_device.h"
#include "vk_cmdbuffer.h"
#include "vk_image.h"

namespace engine{  

inline VkPipelineVertexInputStateCreateInfo vkVertexInputStateCreateInfo() 
{
    VkPipelineVertexInputStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.vertexBindingDescriptionCount = 0;
    info.vertexAttributeDescriptionCount = 0;
    return info;
}

inline VkCommandBufferBeginInfo vkCommandbufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

inline VkDescriptorSetLayoutBinding vkDescriptorsetLayoutBinding(VkDescriptorType type,VkShaderStageFlags stageFlags,uint32_t binding)
{
    VkDescriptorSetLayoutBinding setbind = {};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stageFlags;
    return setbind;
}

inline VkWriteDescriptorSet vkWriteDescriptorBuffer(VkDescriptorType type,VkDescriptorSet dstSet,VkDescriptorBufferInfo* bufferInfo,uint32_t binding)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;
    return write;
}

template<typename T>
inline VkPushConstantRange vkPushConstantRange(uint32 offset,VkShaderStageFlagBits flag)
{
    VkPushConstantRange push_constant = {};
    push_constant.offset = offset;
    push_constant.size = sizeof(T);
    push_constant.stageFlags = flag;
    return push_constant;
}

inline VkRenderPassBeginInfo vkRenderpassBeginInfo(
    VkRenderPass renderPass, 
    VkExtent2D windowExtent, 
    VkFramebuffer framebuffer)
{
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext = nullptr;
    info.renderPass = renderPass;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = windowExtent;
    info.clearValueCount = 1;
    info.pClearValues = nullptr;
    info.framebuffer = framebuffer;
    return info;
}

inline VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo() 
{
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

inline VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule) 
{
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.stage = stage;
    info.module = shaderModule;
    info.pName = "main";
    return info;
}

inline VkPipelineInputAssemblyStateCreateInfo vkInputAssemblyCreateInfo(VkPrimitiveTopology topology) 
{
    VkPipelineInputAssemblyStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.topology = topology;
    info.primitiveRestartEnable = VK_FALSE;
    return info;
}

inline VkPipelineRasterizationStateCreateInfo vkRasterizationStateCreateInfo(VkPolygonMode polygonMode)
{
    VkPipelineRasterizationStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;
    info.polygonMode = polygonMode;
    info.lineWidth = 1.0f;
    info.cullMode = VK_CULL_MODE_NONE;
    info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    info.depthBiasEnable = VK_FALSE;
    info.depthBiasConstantFactor = 0.0f;
    info.depthBiasClamp = 0.0f;
    info.depthBiasSlopeFactor = 0.0f;
    return info;
}

inline VkPipelineMultisampleStateCreateInfo vkMultisamplingStateCreateInfo()
{
    VkPipelineMultisampleStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.sampleShadingEnable = VK_FALSE;
    info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    info.minSampleShading = 1.0f;
    info.pSampleMask = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable = VK_FALSE;
    return info;
}

inline VkPipelineDepthStencilStateCreateInfo vkDepthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
{
    VkPipelineDepthStencilStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
    info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
    info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
    info.depthBoundsTestEnable = VK_FALSE;
    info.minDepthBounds = 0.0f; // Optional
    info.maxDepthBounds = 1.0f; // Optional
    info.stencilTestEnable = VK_FALSE;

    return info;
}

inline VkPipelineColorBlendAttachmentState vkColorBlendAttachmentState() 
{
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    return colorBlendAttachment;
}

struct VulkanVertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct VulkanGraphicsPipelineFactory 
{
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VulkanVertexInputDescription vertexInputDescription;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
};

class VulkanFrameBufferFactory
{
private:
    VkFramebufferCreateInfo m_frameBufferCreateInfo = {};
    std::vector<VkImageView> m_attachments{};
    uint32 m_width  = 0;
    uint32 m_height = 0;

public:
    VulkanFrameBufferFactory()
    {
        m_frameBufferCreateInfo = {};
        m_frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        m_frameBufferCreateInfo.layers = 1;
    }

    VulkanFrameBufferFactory& setRenderpass(VkRenderPass pass)
    {
        m_frameBufferCreateInfo.renderPass = pass;
        return *this;
    }

    VulkanFrameBufferFactory& addArrayAttachment(VulkanImageArray* img,uint32 index)
    {
        m_attachments.push_back(img->getArrayImageView(index));
        if(m_width == 0 || m_height ==0)
        {
            m_width = img->getExtent().width;
            m_height = img->getExtent().height;
        }

        CHECK(m_width != 0 && m_height != 0 && m_width == img->getExtent().width && m_height == img->getExtent().height);
        m_frameBufferCreateInfo.width = m_width;
        m_frameBufferCreateInfo.height = m_height;
        return *this;
    }

    VulkanFrameBufferFactory& addAttachment(VulkanImage* img)
    {
        m_attachments.push_back(img->getImageView());
        if(m_width == 0 || m_height ==0)
        {
            m_width = img->getExtent().width;
            m_height = img->getExtent().height;
        }

        CHECK(m_width != 0 && m_height != 0 && m_width == img->getExtent().width && m_height == img->getExtent().height);
        m_frameBufferCreateInfo.width = m_width;
        m_frameBufferCreateInfo.height = m_height;
        return *this;
    }

    VulkanFrameBufferFactory& addAttachment(VkImageView img,VkExtent2D extent)
    {
        m_attachments.push_back(img);
        if(m_width == 0 || m_height ==0)
        {
            m_width  = extent.width;
            m_height = extent.height;
        }

        CHECK(m_width != 0 && m_height != 0 && m_width == extent.width && m_height == extent.height);
        m_frameBufferCreateInfo.width = m_width;
        m_frameBufferCreateInfo.height = m_height;
        return *this;
    }

    VulkanFrameBufferFactory& clearAttachment()
    {
        m_width = 0;
        m_height = 0;
        m_attachments.resize(0);
        m_frameBufferCreateInfo.width = 0;
        m_frameBufferCreateInfo.height = 0;
        return *this;
    }

    // 按照先前填充的信息创建VkFramebuffer
    VkFramebuffer create(VkDevice dev,bool bClearAfterCreate = true)
    {
        VkFramebuffer value = VK_NULL_HANDLE;
        m_frameBufferCreateInfo.attachmentCount = (uint32)m_attachments.size();
        m_frameBufferCreateInfo.pAttachments = m_attachments.data();
        vkCheck(vkCreateFramebuffer(dev, &m_frameBufferCreateInfo, nullptr, &value));
        if(bClearAfterCreate)
        {
            clearAttachment();
        }
        return value;
    }
};

class VulkanSubmitInfo
{
private:
    VkSubmitInfo submitInfo{};

public:
    VulkanSubmitInfo() 
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.pWaitDstStageMask = waitStages.data();
    }

    operator VkSubmitInfo()
    {
        return submitInfo;
    }

    VkSubmitInfo& get() { return submitInfo; }

    VulkanSubmitInfo& setWaitStage(const std::vector<VkPipelineStageFlags>& wait_stages)
    {
        submitInfo.pWaitDstStageMask = wait_stages.data();
        return *this;
    }
    VulkanSubmitInfo& setWaitStage(std::vector<VkPipelineStageFlags>&& wait_stages) = delete;

    VulkanSubmitInfo& setWaitSemaphore(VkSemaphore* wait,int32 count)
    {
        submitInfo.waitSemaphoreCount = count;
        submitInfo.pWaitSemaphores = wait;
        return *this;
    }

    VulkanSubmitInfo& setSignalSemaphore(VkSemaphore* signal,int32 count)
    {
        submitInfo.signalSemaphoreCount = count;
        submitInfo.pSignalSemaphores = signal;
        return *this;
    }

    VulkanSubmitInfo& setCommandBuffer(VkCommandBuffer* cb,int32 count)
    {
        submitInfo.commandBufferCount = count;
        submitInfo.pCommandBuffers = cb;
        return *this;
    }

    VulkanSubmitInfo& setCommandBuffer(VulkanCommandBuffer* cb,int32 count)
    {
        submitInfo.commandBufferCount = count;
        submitInfo.pCommandBuffers = &cb->getInstance();
        return *this;
    }

    VulkanSubmitInfo& clear()
    {
        submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        std::vector<VkPipelineStageFlags> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages.data();
        return *this;
    }
};

}