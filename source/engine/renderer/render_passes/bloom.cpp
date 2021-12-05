#include "bloom.h"
#include "../renderer.h"

using namespace engine;

struct GpuBlurPushConstants
{
	glm::vec2 direction;
	int32 mipmapLevel;
};

struct GpuBlendPushConstants
{
	float weight;
	float intensity;
};

static AutoCVarFloat cVarBloomIntensity(
	"r.Bloom.Intensity",
	"Bloom intensity",
	"Bloom",
	1.5f,
	CVarFlags::ReadAndWrite
);


void engine::BloomPass::initInner()
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

	// normalize weights
	float n = 0;
	for(uint32_t i = 1; i < (uint32)m_blendWeight.size(); i++)
	{
		n += (float)m_blendWeight[i];
	}
		
	for(uint32_t i = 1; i < (uint32)m_blendWeight.size(); i++)
	{
		m_blendWeight[i] /= n;
	}
}

void engine::BloomPass::beforeSceneTextureRecreate()
{
	destroyFramebuffers();
	destroyPipeline();
}

void engine::BloomPass::afterSceneTextureRecreate()
{
	createFramebuffers();
	createPipeline();
}

void engine::BloomPass::dynamicRecord(uint32 backBufferIndex)
{
	VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
	commandBufBegin(backBufferIndex);
	
	CHECK(m_verticalBlur.size() == g_downsampleCount);
	CHECK(m_verticalBlur.size() == m_horizontalBlur.size());

	uint32 loopWidth  = m_renderScene->getSceneTextures().getLDRSceneColor()->getExtent().width;
	uint32 loopHeight = m_renderScene->getSceneTextures().getLDRSceneColor()->getExtent().height;

	uint32 widthChain[g_downsampleCount + 1] { };
	uint32 heightChain[g_downsampleCount + 1] { };

	for(auto i = 0; i < g_downsampleCount+1; i++)
	{
		widthChain[i] = loopWidth;
		heightChain[i] = loopHeight;

		loopWidth = loopWidth >> 1;
		loopHeight = loopHeight >> 1;
	}

	// from biggest mip to min mip.
	for(int32 loopId = g_downsampleCount - 1; loopId >= 0; loopId--)
	{
		// from mip #5 blur
		// then mix last blend mip.

		VkExtent2D blurExtent2D = { widthChain[loopId + 1], heightChain[loopId +1] };
		CHECK(m_horizontalBlur[loopId]->getInfo().extent.width  == widthChain[loopId + 1]);
		CHECK(m_horizontalBlur[loopId]->getInfo().extent.height == heightChain[loopId +1]);

		// horizonral blur 
		{
			

			VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(
				getRenderpass(),
				blurExtent2D,
				m_horizontalBlurFramebuffers[loopId]
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
			scissor.extent = blurExtent2D;
			scissor.offset = {0,0};
			vkCmdSetScissor(cmd,0,1,&scissor);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)widthChain[loopId + 1];
			viewport.height = (float)heightChain[loopId +1];
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(cmd,0,1,&viewport);
			vkCmdSetDepthBias(cmd,0,0,0);

			vkCmdBindDescriptorSets(
				cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_blurPipelineLayouts[backBufferIndex],
				0, // PassSet #0
				1,
				&m_horizontalBlurDescriptorSets[loopId].set,0,nullptr
			);

			GpuBlurPushConstants blurConst{};
			// blur horizatonal dir
			blurConst.direction = glm::vec2(1.0f/(float)(widthChain[loopId + 1]),0.0f);
			blurConst.mipmapLevel = loopId+1;

			vkCmdPushConstants(cmd,
				m_blurPipelineLayouts[backBufferIndex],
				VK_SHADER_STAGE_FRAGMENT_BIT,0,
				sizeof(GpuBlurPushConstants),
				&blurConst
			);

			vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_blurPipelines[backBufferIndex]);
			vkCmdDraw(cmd,3,1,0,0);
			vkCmdEndRenderPass(cmd);
		}

		CHECK(m_verticalBlur[loopId]->getInfo().extent.width==widthChain[loopId + 1]);
		CHECK(m_verticalBlur[loopId]->getInfo().extent.height==heightChain[loopId + 1]);

		// vertical blur
		{
			VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(
				getRenderpass(),
				blurExtent2D,
				m_verticalBlurFramebuffers[loopId]
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
			scissor.extent = blurExtent2D;
			scissor.offset = {0,0};
			vkCmdSetScissor(cmd,0,1,&scissor);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)widthChain[loopId + 1];
			viewport.height = (float)heightChain[loopId + 1];
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(cmd,0,1,&viewport);
			vkCmdSetDepthBias(cmd,0,0,0);

			vkCmdBindDescriptorSets(
				cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_blurPipelineLayouts[backBufferIndex],
				0,
				1,
				&m_verticalBlurDescriptorSets[loopId].set,0,nullptr
			);

			GpuBlurPushConstants blurConst{};

			// blur vertical direction
			blurConst.direction = glm::vec2(0.0f, 1.0f/(float)(heightChain[loopId + 1]));
			blurConst.mipmapLevel = loopId + 1;

			vkCmdPushConstants(cmd,
				m_blurPipelineLayouts[backBufferIndex],
				VK_SHADER_STAGE_FRAGMENT_BIT,0,
				sizeof(GpuBlurPushConstants),
				&blurConst
			);

			vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_blurPipelines[backBufferIndex]);
			vkCmdDraw(cmd,3,1,0,0);
			vkCmdEndRenderPass(cmd);
		}

		// upscale sample
		VkExtent2D blendExtent2D = { widthChain[loopId], heightChain[loopId] };
		
		// blend with last mip
		{
			VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(
				getRenderpass(),
				blendExtent2D,
				m_blendFramebuffers[loopId]
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
			scissor.extent = blendExtent2D;
			scissor.offset = {0,0};
			vkCmdSetScissor(cmd,0,1,&scissor);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)widthChain[loopId];
			viewport.height = (float)heightChain[loopId];
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(cmd,0,1,&viewport);
			vkCmdSetDepthBias(cmd,0,0,0);

			vkCmdBindDescriptorSets(
				cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_blendPipelineLayouts[backBufferIndex],
				0,
				1,
				&m_blendDescriptorSets[loopId].set,0,nullptr
			);

			float blendConstants[4] = { 
				m_blendWeight[loopId], 
				m_blendWeight[loopId], 
				m_blendWeight[loopId], 
				m_blendWeight[loopId] 
			};

			vkCmdSetBlendConstants(cmd, blendConstants);

			GpuBlendPushConstants blendConst{};
			blendConst.weight = loopId == 0 ? 1.0f - m_blendWeight[0] : 1.0f;
			blendConst.intensity = cVarBloomIntensity.get();
			vkCmdPushConstants(cmd,
				m_blendPipelineLayouts[backBufferIndex],
				VK_SHADER_STAGE_FRAGMENT_BIT,0,
				sizeof(GpuBlendPushConstants),
				&blendConst
			);

			vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_blendPipelines[backBufferIndex]);
			vkCmdDraw(cmd,3,1,0,0);
			vkCmdEndRenderPass(cmd);
		}
	}

	commandBufEnd(backBufferIndex);
}

void BloomPass::createRenderpass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = SceneTextures::getHDRSceneColorFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

	std::array<VkSubpassDependency,2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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

void BloomPass::destroyRenderpass()
{
	VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void BloomPass::createPipeline()
{
	if(bInitPipeline) return;

#pragma region descriptor set
	// create for horizontal blur 
	{
		RenderTexture* downsampleChain = static_cast<RenderTexture*>(
			m_renderScene->getSceneTextures().getDownSampleChain()
		);

		for(uint32 index = 0; index < m_horizontalBlurDescriptorSets.size(); index++)
		{
			VkDescriptorImageInfo horizontalBlurInputImage = {};
			horizontalBlurInputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			horizontalBlurInputImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();

			// horizatonal input mip #1 from down with blur blend
			//					 mip #2 from down with blur blend
			//					 mip #3 from down with blur blend
			//					 mip #4 from down with blur blend
			//					 mip #5 from down
			horizontalBlurInputImage.imageView = downsampleChain->getMipmapView(index);
			
			// all bloom descriptor set layout same so just init one.
			if(index == 0)
			{
				m_renderer->vkDynamicDescriptorFactoryBegin(0)
					.bindImage(0,&horizontalBlurInputImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT)
					.build(m_horizontalBlurDescriptorSets[index],m_descriptorSetLayout);
			}
			else
			{
				m_renderer->vkDynamicDescriptorFactoryBegin(0)
					.bindImage(0,&horizontalBlurInputImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT)
					.build(m_horizontalBlurDescriptorSets[index]);
			}
		}
	}

	// create for vertical blur
	{
		for(uint32 index = 0; index < m_verticalBlurDescriptorSets.size(); index++)
		{
			VkDescriptorImageInfo verticalBlurInputImage = {};
			verticalBlurInputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			verticalBlurInputImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();

			// vertical input horizontal blur mip #1
			//				  horizontal blur mip #2
			//				  horizontal blur mip #3
			//				  horizontal blur mip #4
			//				  horizontal blur mip #5
			verticalBlurInputImage.imageView = m_horizontalBlur[index]->getImageView();

			m_renderer->vkDynamicDescriptorFactoryBegin(0)
				.bindImage(0,&verticalBlurInputImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT)
				.build(m_verticalBlurDescriptorSets[index]);
		}
	}

	// create for blend
	{
		for(uint32 index = 0; index < m_blendDescriptorSets.size(); index++)
		{
			VkDescriptorImageInfo blendInputImage = {};
			blendInputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			blendInputImage.sampler = VulkanRHI::get()->getLinearClampNoMipSampler();

			// blend input vertical blur mip #1
			//			   vertical blur mip #2
			//			   vertical blur mip #3
			//			   vertical blur mip #4
			//			   vertical blur mip #5
			blendInputImage.imageView = m_verticalBlur[index]->getImageView();

			m_renderer->vkDynamicDescriptorFactoryBegin(0)
				.bindImage(0,&blendInputImage,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT)
				.build(m_blendDescriptorSets[index]);
		}
	}
#pragma endregion

#pragma region pipeline
	uint32 swapchainCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	// blur pipeline
	{
		m_blurPipelines.resize(swapchainCount);
		m_blurPipelineLayouts.resize(swapchainCount);

		for(uint32 index = 0; index < swapchainCount; index++)
		{
			VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

			VkPushConstantRange push_constant{};
			push_constant.offset = 0;
			push_constant.size = sizeof(GpuBlurPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			plci.pPushConstantRanges = &push_constant;
			plci.pushConstantRangeCount = 1;

			std::vector<VkDescriptorSetLayout> setLayouts = 
			{
				m_descriptorSetLayout.layout
			};

			plci.setLayoutCount = (uint32)setLayouts.size();
			plci.pSetLayouts = setLayouts.data();

			m_blurPipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

			VulkanGraphicsPipelineFactory gpf = {};

			auto* vertShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/fullscreen.vert.spv");
			auto* fragShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/blur.frag.spv");

			gpf.shaderStages.clear();
			gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,*vertShader));
			gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,*fragShader));
			gpf.vertexInputInfo = vkVertexInputStateCreateInfo();

			gpf.vertexInputInfo.vertexBindingDescriptionCount = 0;
			gpf.vertexInputInfo.pVertexBindingDescriptions = nullptr;
			gpf.vertexInputInfo.vertexAttributeDescriptionCount = 0;
			gpf.vertexInputInfo.pVertexAttributeDescriptions = nullptr;

			gpf.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			gpf.depthStencil = vkDepthStencilCreateInfo(false,false,VK_COMPARE_OP_ALWAYS);

			gpf.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
			gpf.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			gpf.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

			gpf.multisampling = vkMultisamplingStateCreateInfo();
			gpf.colorBlendAttachments = { vkColorBlendAttachmentState() };

			// we don't care about extent here. just use default.
			// we will set dynamic when we need.
			gpf.viewport.x = 0.0f;
			gpf.viewport.y = 0.0f;
			gpf.viewport.width = 10.0f;
			gpf.viewport.height = 10.0f;
			gpf.viewport.minDepth = 0.0f;
			gpf.viewport.maxDepth = 1.0f;
			gpf.scissor.offset = {0, 0};
			gpf.scissor.extent = {10u,10u};

			gpf.pipelineLayout = m_blurPipelineLayouts[index];
			m_blurPipelines[index] = gpf.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),m_renderpass);
		}
	}

	// blend pipeline
	{
		m_blendPipelines.resize(swapchainCount);
		m_blendPipelineLayouts.resize(swapchainCount);

		for(uint32 index = 0; index < swapchainCount; index++)
		{
			VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

			VkPushConstantRange push_constant{};
			push_constant.offset = 0;
			push_constant.size = sizeof(GpuBlendPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			plci.pPushConstantRanges = &push_constant;
			plci.pushConstantRangeCount = 1;

			std::vector<VkDescriptorSetLayout> setLayouts = 
			{
				m_descriptorSetLayout.layout
			};

			plci.setLayoutCount = (uint32)setLayouts.size();
			plci.pSetLayouts = setLayouts.data();

			m_blendPipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

			VulkanGraphicsPipelineFactory gpf = {};

			auto* vertShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/fullscreen.vert.spv");
			auto* fragShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/blend.frag.spv");

			gpf.shaderStages.clear();
			gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,*vertShader));
			gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,*fragShader));
			gpf.vertexInputInfo = vkVertexInputStateCreateInfo();

			gpf.vertexInputInfo.vertexBindingDescriptionCount = 0;
			gpf.vertexInputInfo.pVertexBindingDescriptions = nullptr;
			gpf.vertexInputInfo.vertexAttributeDescriptionCount = 0;
			gpf.vertexInputInfo.pVertexAttributeDescriptions = nullptr;

			gpf.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			gpf.depthStencil = vkDepthStencilCreateInfo(false,false,VK_COMPARE_OP_ALWAYS);

			gpf.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
			gpf.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			gpf.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

			gpf.multisampling = vkMultisamplingStateCreateInfo();

			// blend add.
			VkPipelineColorBlendAttachmentState att_state[1] {};
			att_state[0].colorWriteMask = 0xf;
			att_state[0].blendEnable = VK_TRUE;
			att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
			att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
			att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
			att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

			VkPipelineColorBlendStateCreateInfo cb {};
			cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			cb.flags = 0;
			cb.pNext = NULL;
			cb.attachmentCount = 1;
			cb.pAttachments = att_state;
			cb.logicOpEnable = VK_FALSE;
			cb.logicOp = VK_LOGIC_OP_NO_OP;
			cb.blendConstants[0] = 1.0f;
			cb.blendConstants[1] = 1.0f;
			cb.blendConstants[2] = 1.0f;
			cb.blendConstants[3] = 1.0f;

			gpf.colorBlendAttachments = { att_state[0] };

			// we don't care about extent here. just use default.
			// we will set dynamic when we need.
			gpf.viewport.x = 0.0f;
			gpf.viewport.y = 0.0f;
			gpf.viewport.width = 10.0f;
			gpf.viewport.height = 10.0f;
			gpf.viewport.minDepth = 0.0f;
			gpf.viewport.maxDepth = 1.0f;
			gpf.scissor.offset = {0, 0};
			gpf.scissor.extent = {10u,10u};

			gpf.pipelineLayout = m_blendPipelineLayouts[index];
			m_blendPipelines[index] = gpf.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),m_renderpass, cb);
		}
	}
#pragma endregion

	bInitPipeline = true;
}

void BloomPass::destroyPipeline()
{
	if(!bInitPipeline) return;

	for(uint32 index = 0; index < m_blurPipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_blurPipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_blurPipelineLayouts[index],nullptr);
	}
	m_blurPipelines.resize(0);
	m_blurPipelineLayouts.resize(0);

	for(uint32 index = 0; index < m_blendPipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_blendPipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_blendPipelineLayouts[index],nullptr);
	}
	m_blendPipelines.resize(0);
	m_blendPipelineLayouts.resize(0);

	bInitPipeline = false;
}

void engine::BloomPass::createFramebuffers()
{
	// create local image for bloom tmp rt.
#pragma region create rt image
	// horizontal blur image
	{
		uint32 width = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().width;
		uint32 height = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().height;

		for(uint32 index = 0; index < m_horizontalBlur.size(); index++)
		{
			/**
			**  #0 is mip1, #1 is mip2, #2 is mip3, #3 is mip4, #4 is mip5 
			**  so just >> 1 to get actual size.   
			*/

			width  = width  >> 1;
			height = height >> 1;

			// min scene texture size is (64u,64u) = (2^6, 2^6).
			// so there should always pass.
			CHECK(width  >= 1);
			CHECK(height >= 1);

			CHECK(m_horizontalBlur[index] == nullptr);

			m_horizontalBlur[index] = RenderTexture::create(
				VulkanRHI::get()->getVulkanDevice(),
				width,
				height,
				SceneTextures::getHDRSceneColorFormat()
			);
		}
	}

	// vertical blur image
	{
		uint32 width = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().width;
		uint32 height = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().height;

		for(uint32 index = 0; index < m_verticalBlur.size(); index++)
		{
			/**
			**  #0 is mip1, #1 is mip2, #2 is mip3, #3 is mip4, #4 is mip5 
			**  so just >> 1 to get actual size.   
			*/

			width  = width  >> 1;
			height = height >> 1;

			// min scene texture size is (64u,64u) = (2^6, 2^6).
			// so there should always pass.
			CHECK(width  >= 1);
			CHECK(height >= 1);

			CHECK(m_verticalBlur[index] == nullptr);

			m_verticalBlur[index] = RenderTexture::create(
				VulkanRHI::get()->getVulkanDevice(),
				width,
				height,
				SceneTextures::getHDRSceneColorFormat()
			);
		}
	}
#pragma endregion

	// create framebuffers.
#pragma region create framebuffer
	{
		// create horizontal framebuffer.
		{
			uint32 width = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().width;
			uint32 height = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().height;

			for(uint32 i = 0; i < m_horizontalBlurFramebuffers.size(); i++)
			{
				/**
				**  #0 is mip1, #1 is mip2, #2 is mip3, #3 is mip4, #4 is mip5 
				**  so just >> 1 to get actual size.   
				*/

				width  = width  >> 1;
				height = height >> 1;

				// min scene texture size is (64u,64u) = (2^6, 2^6).
				// so there should always pass.
				CHECK(width  >= 1);
				CHECK(height >= 1);

				VkExtent2D extent2D { width, height };

				VulkanFrameBufferFactory fbf {};
				fbf.setRenderpass(m_renderpass)
					.addAttachment(
						m_horizontalBlur[i]->getImageView(), // we use horizontal blur image as rt.
						extent2D
				);

				m_horizontalBlurFramebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
			}
		}

		// create vertical framebuffer
		{
			uint32 width = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().width;
			uint32 height = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().height;

			for(uint32 i = 0; i < m_verticalBlurFramebuffers.size(); i++)
			{
				/**
				**  #0 is mip1, #1 is mip2, #2 is mip3, #3 is mip4, #4 is mip5 
				**  so just >> 1 to get actual size.   
				*/

				width  = width  >> 1;
				height = height >> 1;

				// min scene texture size is (64u,64u) = (2^6, 2^6).
				// so there should always pass.
				CHECK(width  >= 1);
				CHECK(height >= 1);

				VkExtent2D extent2D { width, height };

				VulkanFrameBufferFactory fbf {};
				fbf.setRenderpass(m_renderpass)
					.addAttachment(
						m_verticalBlur[i]->getImageView(), // we use vertical blur image as rt.
						extent2D
					);

				m_verticalBlurFramebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
			}
		}

		// create blend framebuffer
		{
			uint32 width = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().width;
			uint32 height = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent().height;

			RenderTexture* downsampleChain = static_cast<RenderTexture*>(
				m_renderScene->getSceneTextures().getDownSampleChain()
			);

			for(uint32 i = 0; i < g_downsampleCount; i++)
			{
				//#0 is mip0, #1 is mip1, #2 is mip2, #3 is mip3, #4 is mip4 

				// min scene texture size is (64u,64u) = (2^6, 2^6).
				// so there should always pass.
				CHECK(width  >= 1);
				CHECK(height >= 1);

				VkExtent2D extent2D { width, height };

				VulkanFrameBufferFactory fbf {};

				if(i == 0)
				{
					fbf.setRenderpass(m_renderpass)
						.addAttachment(
							// we use hdr image as rt.
							m_renderScene->getSceneTextures().getHDRSceneColor()->getImageView(), 
							extent2D
						);
				}
				else
				{
					fbf.setRenderpass(m_renderpass)
						.addAttachment(
							// we use bloom blur image as rt.
							downsampleChain->getMipmapView(i - 1), 
							extent2D
						);
				}

				m_blendFramebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());

				width  = width  >> 1;
				height = height >> 1;
			}
		}
	}

#pragma endregion
}

void engine::BloomPass::destroyFramebuffers()
{
	// destroy image first
	{
		for(uint32 i = 0; i < m_horizontalBlur.size(); i++)
		{
			delete m_horizontalBlur[i];
			m_horizontalBlur[i] = nullptr;
		}

		for(uint32 i = 0; i < m_verticalBlur.size(); i++)
		{
			delete m_verticalBlur[i];
			m_verticalBlur[i] = nullptr;
		}
	}

	// then destroy frame buffers
	{
		for(uint32 i = 0; i < m_horizontalBlurFramebuffers.size(); i++)
		{
			VulkanRHI::get()->destroyFramebuffer(m_horizontalBlurFramebuffers[i]);
			m_horizontalBlurFramebuffers[i] = VK_NULL_HANDLE;
		}

		for(uint32 i = 0; i < m_verticalBlurFramebuffers.size(); i++)
		{
			VulkanRHI::get()->destroyFramebuffer(m_verticalBlurFramebuffers[i]);
			m_verticalBlurFramebuffers[i] = VK_NULL_HANDLE;
		}

		for(uint32 i = 0; i < m_blendFramebuffers.size(); i++)
		{
			VulkanRHI::get()->destroyFramebuffer(m_blendFramebuffers[i]);
			m_blendFramebuffers[i] = VK_NULL_HANDLE;
		}
	}

}
