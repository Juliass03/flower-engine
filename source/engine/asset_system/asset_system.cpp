#include "asset_system.h"
#include "../core/file_system.h"
#include <filesystem>
#include <fstream>
#include "../vk/vk_rhi.h"
#include <stb/stb_image.h>
#include "../../imgui/imgui_impl_vulkan.h"
#include "../renderer/texture.h"
#include "asset_texture.h"
#include "asset_mesh.h"
#include "../core/job_system.h"

bool engine::asset_system::g_assetFolderDirty = false;

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

	iconFolder = new IconInfo(mediaDir + "icon/folder.png");
	iconFile = new IconInfo(mediaDir + "icon/file.png");
	iconBack = new IconInfo(mediaDir + "icon/back.png");
	iconHome = new IconInfo(mediaDir + "icon/home.png");
	iconFlash = new IconInfo(mediaDir + "icon/flash.png");
	iconProject = new IconInfo(mediaDir + "icon/project.png");
	iconMaterial = new IconInfo(mediaDir + "icon/material.png");
	iconMesh = new IconInfo(mediaDir + "icon/mesh.png");
	iconTexture = new IconInfo(mediaDir + "icon/image.png");

	bInit = true;
}

void asset_system::EngineAsset::release()
{
	if(!bInit) return;
	
	delete iconFolder;
	delete iconFile;
	delete iconBack;
	delete iconHome;
	delete iconFlash;
	delete iconProject;
	delete iconMesh;
	delete iconMaterial;
	delete iconTexture;
	bInit = false;
}

AssetSystem::AssetSystem(Ref<ModuleManager> in): IRuntimeModule(in) {  }

bool AssetSystem::init()
{
	loadEngineTextures();
	processProjectDirectory();
	return true;
}

void AssetSystem::tick(float dt)
{
	
}

void AssetSystem::release()
{

}

bool AssetSystem::loadTexture2DImage(CombineTexture& inout,const std::string& gameName,std::function<void(CombineTexture loadResult)>&& loadedCallback)
{
	bool result = false;

	if(!result)
	{
		inout = TextureLibrary::get()->m_textureContainer[s_defaultCheckboardTextureName];
	}

	return result;
}

bool AssetSystem::loadMesh(Mesh& inout,const std::string& gameName,std::function<void(Mesh* loadResult)>&& loadedCallback)
{
	

	return false;
}

// NOTE: 扫描项目文件夹里的资源
//       建立meta资源索引
void AssetSystem::processProjectDirectory()
{
	jobsystem::execute([this](){
		namespace fs = std::filesystem;
		const std::string projectPath = s_projectDir;
		for(const auto& entry:fs::recursive_directory_iterator(projectPath))
		{
			if(!fs::is_directory(entry.path()))
			{
				std::string pathStr = FileSystem::toCommonPath(entry);
				std::string suffixStr = FileSystem::getFileSuffixName(pathStr);
				std::string rawName = FileSystem::getFileRawName(pathStr);
				std::string fileName = FileSystem::getFileNameWithoutSuffix(pathStr);

				if(suffixStr.find(".tga")!=std::string::npos)
				{
					jobsystem::execute([this,pathStr]()
					{
						addAsset(pathStr,EAssetFormat::T_R8G8B8A8);
					});
				}
				else if(suffixStr.find(".obj")!=std::string::npos)
				{
					jobsystem::execute([this,pathStr]()
					{
						addAsset(pathStr,EAssetFormat::M_StaticMesh_Obj);
					});
				}
			}
		}
	});
}

void AssetSystem::addAsset(std::string path,EAssetFormat format)
{
	switch(format)
	{
	case engine::asset_system::EAssetFormat::T_R8G8B8A8:
	{
		LOG_IO_INFO("Bakeing texture {0}....",path);
		std::string bakeName = rawPathToAssetPath(path,format);
		bakeTexture(path.c_str(),bakeName.c_str(),true,true,4);
		LOG_IO_INFO("Baked texture {0}.",bakeName);
		remove(path.c_str());
		g_assetFolderDirty = true;
		return;
	}
	case engine::asset_system::EAssetFormat::M_StaticMesh_Obj:
	{
		LOG_IO_INFO("Baking static mesh {0}...",path);
		std::string bakeName = rawPathToAssetPath(path,format);
		bakeObjMesh(path.c_str(),bakeName.c_str(),"",true);
		LOG_IO_INFO("Baked static mesh {0}.",bakeName);
		remove(path.c_str());
		g_assetFolderDirty = true;

		return;
	}
	case engine::asset_system::EAssetFormat::Unknown:
	default:
		LOG_FATAL("Unkonw asset type!");
		return;
	}
}

void AssetSystem::loadEngineTextures()
{
	auto* textureLibrary = TextureLibrary::get();

	textureLibrary->m_textureContainer[s_defaultWhiteTextureName] = {};
	auto& whiteTexture = textureLibrary->m_textureContainer[s_defaultWhiteTextureName];
	whiteTexture.sampler = VulkanRHI::get()->getLinearClampSampler();
	whiteTexture.texture = loadFromFile(s_defaultWhiteTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);

	textureLibrary->m_textureContainer[s_defaultBlackTextureName] = {};
	auto& blackTexture = textureLibrary->m_textureContainer[s_defaultBlackTextureName];
	blackTexture.sampler = VulkanRHI::get()->getLinearClampSampler();
	blackTexture.texture = loadFromFile(s_defaultBlackTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);

	textureLibrary->m_textureContainer[s_defaultNormalTextureName] = {};
	auto& normalTexture = textureLibrary->m_textureContainer[s_defaultNormalTextureName];
	normalTexture.sampler = VulkanRHI::get()->getLinearClampSampler();
	normalTexture.texture = loadFromFile(s_defaultNormalTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);

	textureLibrary->m_textureContainer[s_defaultCheckboardTextureName] = {};
	auto& checkboxTexture = textureLibrary->m_textureContainer[s_defaultCheckboardTextureName];
	checkboxTexture.sampler = VulkanRHI::get()->getLinearClampSampler();
	checkboxTexture.texture = loadFromFile(s_defaultCheckboardTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);

	textureLibrary->m_textureContainer[s_defaultEmissiveTextureName] = {};
	auto& emissiveTexture = textureLibrary->m_textureContainer[s_defaultEmissiveTextureName];
	emissiveTexture.sampler = VulkanRHI::get()->getLinearClampSampler();
	emissiveTexture.texture = loadFromFile(s_defaultEmissiveTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);
}
}

void asset_system::EngineAsset::IconInfo::init(const std::string& path,bool flip)
{
	icon = loadFromFile(path, VK_FORMAT_R8G8B8A8_SRGB, 4,flip);
	sampler = VulkanRHI::get()->getLinearClampSampler();
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