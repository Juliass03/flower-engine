#include "asset_system.h"
#include "../core/file_system.h"
#include <filesystem>
#include <fstream>
#include "../vk/vk_rhi.h"
#include <stb/stb_image.h>
#include "../../imgui/imgui_impl_vulkan.h"

namespace engine{ namespace asset_system{

EngineAsset* EngineAsset::s_asset = new EngineAsset();

Texture2DImage* loadFromFile(const std::string& path,VkFormat format,uint32 req,bool flip)
{
	int32 texWidth, texHeight, texChannels;
	stbi_set_flip_vertically_on_load(flip);  
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,req);

	if (!pixels) 
	{
		LOG_IO_FATAL("Fail to load image {0}.",path);
	}

	int32 imageSize = texWidth * texHeight * req;

	std::vector<uint8> pixelData{};
	pixelData.resize(imageSize);

	memcpy(pixelData.data(),pixels,imageSize);

	stbi_image_free(pixels);
	stbi_set_flip_vertically_on_load(false);  

	Texture2DImage* ret = Texture2DImage::createAndUpload(
		VulkanRHI::get()->getVulkanDevice(),
		VulkanRHI::get()->getGraphicsCommandPool(),
		VulkanRHI::get()->getGraphicsQueue(),
		pixelData,
		texWidth,texHeight,VK_IMAGE_ASPECT_COLOR_BIT,format
	);

	return ret;
}

void asset_system::EngineAsset::init()
{
	if(bInit) return;
	std::string mediaDir = s_mediaDir; 

	iconFolder.init(mediaDir + "icon/folder.png");
	iconFile.init(mediaDir + "icon/file.png");
	iconBack.init(mediaDir + "icon/back.png");
	iconHome.init(mediaDir + "icon/home.png");
	iconFlash.init(mediaDir + "icon/flash.png");

	bInit = true;
}

void asset_system::EngineAsset::release()
{
	if(!bInit) return;
	
	iconFolder.release();
	iconFile.release();
	iconBack.release();
	iconHome.release();
	iconFlash.release();
	bInit = false;
}

AssetSystem::AssetSystem(Ref<ModuleManager> in): IRuntimeModule(in) {  }

bool AssetSystem::init()
{


	return true;
}

void AssetSystem::tick(float dt)
{
	
}

void AssetSystem::release()
{

}

void AssetSystem::scanProject()
{

}

}
void asset_system::EngineAsset::IconInfo::init(const std::string& path,bool flip)
{
	icon = loadFromFile(path, VK_FORMAT_R8G8B8A8_SRGB, 4,flip);

	SamplerFactory sfa{};
	sfa
		.MagFilter(VK_FILTER_LINEAR)
		.MinFilter(VK_FILTER_LINEAR)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.AddressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
		.AddressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
		.AddressModeW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
		.CompareOp(VK_COMPARE_OP_LESS)
		.CompareEnable(VK_FALSE)
		.BorderColor(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
		.UnnormalizedCoordinates(VK_FALSE)
		.MaxAnisotropy(1.0f)
		.AnisotropyEnable(VK_FALSE)
		.MinLod(0.0f)
		.MaxLod(0.0f)
		.MipLodBias(0.0f);

	sampler = VulkanRHI::get()->createSampler(sfa.getCreateInfo());
}

void asset_system::EngineAsset::IconInfo::release()
{
	delete icon;
}

void* asset_system::EngineAsset::IconInfo::getId()
{
	if(cacheId!=nullptr)
	{
		return cacheId;
	}
	else
	{
		cacheId = (void*)ImGui_ImplVulkan_AddTexture(sampler,icon->getImageView(),icon->getCurentLayout());
		return cacheId;
	}
}

}