#include "asset_system.h"
#include "../core/file_system.h"
#include <filesystem>
#include <fstream>
#include "../vk/vk_rhi.h"
#include <stb/stb_image.h>

namespace engine{ namespace asset_system{

EngineAsset* EngineAsset::s_asset = new EngineAsset();

Texture2DImage* loadFromFile(const std::string& path,VkFormat format,uint32 req)
{
	int32 texWidth, texHeight, texChannels;
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

	iconError = loadFromFile(mediaDir + "icon/error.png",VK_FORMAT_R8G8B8A8_SRGB, 4);
	iconInfo = loadFromFile(mediaDir + "icon/info.png",VK_FORMAT_R8G8B8A8_SRGB, 4);
	iconNotify = loadFromFile(mediaDir + "icon/notify.png",VK_FORMAT_R8G8B8A8_SRGB, 4);
	iconWarn = loadFromFile(mediaDir + "icon/other.png",VK_FORMAT_R8G8B8A8_SRGB, 4);
	iconOther = loadFromFile(mediaDir + "icon/warn.png",VK_FORMAT_R8G8B8A8_SRGB, 4);

	bInit = true;
}

void asset_system::EngineAsset::release()
{
	if(!bInit) return;
	
	iconError->release();
	iconInfo->release();
	iconNotify->release();
	iconWarn->release();
	iconOther->release();

	delete iconError;
	delete iconInfo;
	delete iconNotify;
	delete iconWarn;
	delete iconOther;

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

}}