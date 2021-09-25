#pragma once
#include "../vk/vk_rhi.h"

namespace engine{

namespace ImGuiStyleEx
{
	constexpr float imageButtonSize = 14.0f;
};

class ImguiPass
{
	struct ImguiPassGpuResource
	{
		VkDescriptorPool descriptorPool;
		VkRenderPass renderPass;

		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkCommandPool> commandPools;
		std::vector<VkCommandBuffer> commandBuffers;
	}; 
	
private:
	bool m_init = false;
	ImguiPassGpuResource m_gpuResource;
	glm::vec4 m_clearColor {0.45f,0.55f,0.60f,1.00f};
	
private:
	void renderpassBuild();
	void renderpassRelease();

public:
	VkCommandBuffer getCommandBuffer(uint32 index)
	{
		return m_gpuResource.commandBuffers[index];
	}


public:
	~ImguiPass() = default;

	void initImgui();
	void release();

	void newFrame();
	void renderFrame(uint32 backBufferIndex);
	void updateAfterSubmit();

};

}