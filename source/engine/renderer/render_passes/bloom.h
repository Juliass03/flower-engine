#pragma once
#include <array>
#include "../pass_interface.h"

namespace engine
{
	
	class BloomPass: public GraphicsPass
	{
	public:
		BloomPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name)
			:GraphicsPass(renderer,scene,sc,name,shaderCompiler::EShaderPass::Unknow)
		{
			m_horizontalBlur = { nullptr, nullptr, nullptr, nullptr, nullptr };
			m_verticalBlur = { nullptr, nullptr, nullptr, nullptr, nullptr };
		}

		virtual void initInner() override;
		virtual void beforeSceneTextureRecreate() override;
		virtual void afterSceneTextureRecreate() override;
		virtual void dynamicRecord(uint32 backBufferIndex) override;

	private:
		// horizontal blur image result.
		// mip 5. 4. 3. 2. 1.
		std::array<VulkanImage*,g_downsampleCount> m_horizontalBlur {};

		// vertical blur image result.
		// mip 5. 4. 3. 2. 1.
		std::array<VulkanImage*,g_downsampleCount> m_verticalBlur {};

		std::array<float, g_downsampleCount + 1> m_blendWeight = {
			1.0f - 0.08f, // 0
			0.25f,        // 1
			0.75f,        // 2
			1.5f,         // 3
			2.5f,         // 4
			3.0f          // 5
		};

	private:
		bool bInitPipeline = false;

		void createRenderpass();
		void destroyRenderpass();

		std::vector<VkPipeline> m_blurPipelines = {};
		std::vector<VkPipelineLayout> m_blurPipelineLayouts = {};

		VulkanDescriptorLayoutReference m_descriptorSetLayout {};

		std::array<VulkanDescriptorSetReference, g_downsampleCount> m_horizontalBlurDescriptorSets = {}; // 
		std::array<VulkanDescriptorSetReference, g_downsampleCount> m_verticalBlurDescriptorSets = {};

		std::vector<VkPipeline> m_blendPipelines = {};
		std::vector<VkPipelineLayout> m_blendPipelineLayouts = {};
		std::array<VulkanDescriptorSetReference, g_downsampleCount> m_blendDescriptorSets = {};

		void createPipeline();
		void destroyPipeline();

		std::array<VkFramebuffer, g_downsampleCount> m_verticalBlurFramebuffers = {};
		std::array<VkFramebuffer, g_downsampleCount> m_horizontalBlurFramebuffers = {};
		std::array<VkFramebuffer, g_downsampleCount> m_blendFramebuffers = {};
		
		void createFramebuffers();
		void destroyFramebuffers();
	};
}