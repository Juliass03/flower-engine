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
#include "../launch/launch_engine_loop.h"
#include "../core/job_system.h"

bool engine::asset_system::g_assetFolderDirty = false;

namespace engine{ namespace asset_system{

EngineAsset* EngineAsset::s_asset = new EngineAsset();
AssetCache* AssetCache::s_cache = new AssetCache();

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

EngineAsset::IconInfo* EngineAsset::getIconInfo(const std::string& name)
{
	if(m_cacheIconInfo[name].icon == nullptr)
	{
		if(TextureLibrary::get()->existTexture(name))
		{
			auto pair = TextureLibrary::get()->getCombineTextureByName(name);
			if(pair.first == ERequestTextureResult::Ready)
			{
				m_cacheIconInfo[name].icon = pair.second.texture;
				m_cacheIconInfo[name].sampler = pair.second.sampler;
			}
		}
	}
	return &m_cacheIconInfo[name];
}

AssetSystem::AssetSystem(Ref<ModuleManager> in): IRuntimeModule(in) {  }

bool AssetSystem::init()
{
	loadEngineTextures();
	processProjectDirectory(true);
	return true;
}

void AssetSystem::tick(float dt)
{
	constexpr int32 scanTime = 1000;
	static int32 scanTick = 0;
	int32 fps = static_cast<int32>(getFps());
	fps = fps > 0 ? fps : scanTime;

	if(scanTick >= fps * 4)
	{
		scanTick = 0;
		processProjectDirectory(false);
	}
	else
	{
		scanTick ++;
	}

	prepareTextureLoad();
	checkTextureUploadStateAsync();
}

void AssetSystem::release()
{

}

void AssetSystem::addLoadTextureTask(const std::string& pathStr)
{
	if(m_texturesNameLoad.find(pathStr)==m_texturesNameLoad.end())
	{
		m_perScanAdditionalTextures.push_back(pathStr);
		m_texturesNameLoad.emplace(pathStr);
	}
}

VkSampler toVkSampler(asset_system::ESamplerType type)
{
	switch(type)
	{
	case engine::asset_system::ESamplerType::PointClamp:
		return VulkanRHI::get()->getPointClampSampler();

	case engine::asset_system::ESamplerType::PointRepeat:
		return VulkanRHI::get()->getPointRepeatSampler();

	case engine::asset_system::ESamplerType::LinearClamp:
		return VulkanRHI::get()->getLinearClampSampler();

	case engine::asset_system::ESamplerType::LinearRepeat:
		return VulkanRHI::get()->getLinearRepeatSampler();
	}
	return VulkanRHI::get()->getPointClampSampler();
}

void AssetSystem::prepareTextureLoad()
{
	for(auto& path : m_perScanAdditionalTextures)
	{
		m_loadingTextureTasks.push(path);
	}
	m_perScanAdditionalTextures.resize(0);

	// NOTE: 由于在运行时生成mipmap，需要在Graphics队列中执行Blit命令
	//       如果我在另外一个线程中提交命令，会产生多线程写入队列的竞争
	//       而且我不想给Graphics队列加锁
	//       因此只好每帧分担一部分任务了
	constexpr auto perTickLoadNum = 100;
	for(auto i = 0; i<perTickLoadNum; i++)
	{
		if(m_loadingTextureTasks.size()==0)
		{
			break;
		}
		auto& path = m_loadingTextureTasks.front();
		auto& textureContainer = TextureLibrary::s_textureLibrary->m_textureContainer;
		CHECK(textureContainer.find(path) == textureContainer.end());
		textureContainer[path] = {};
		loadTexture2DImage(textureContainer[path],path);
		m_loadingTextureTasks.pop();
	}
}

inline void submitAsync(
	VkDevice device,
	VkCommandPool commandPool,
	VkQueue queue,
	VkFence fence,
	const std::function<void(VkCommandBuffer cb)>& func)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device,&allocInfo,&commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer,&beginInfo);
	func(commandBuffer);

	vkEndCommandBuffer(commandBuffer);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(queue,1,&submitInfo,fence);
	vkFreeCommandBuffers(device,commandPool,1,&commandBuffer);
}

// NOTE: 一个Uploader对应一个纹理上传任务
class GpuUploadTextureAsync
{
	bool m_ready = false;
	VkCommandPool m_pool = VK_NULL_HANDLE;
	
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_queue = VK_NULL_HANDLE;
	void release()
	{
		if(fence!=nullptr)
		{
			VulkanRHI::get()->getFencePool().waitAndReleaseFence(fence,1000000);
			fence = nullptr;
		}
		if(uploadBuffers.size()>0)
		{
			for(auto* buf : uploadBuffers)
			{
				delete buf;
				buf = nullptr;
			}
			uploadBuffers.clear();
			uploadBuffers.resize(0);
		}
		finishCallBack = {};
		if(cmdBuf!=VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(m_device,m_pool,1,&cmdBuf);
			cmdBuf = VK_NULL_HANDLE;
		}
	}
public:
	Ref<VulkanFence> fence = nullptr;
	std::vector<VulkanBuffer*> uploadBuffers{};
	std::function<void()> finishCallBack{};
	VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

	void init(VkCommandPool inPool,VkDevice inDevice,VkQueue inQueue)
	{
		release();
		m_device = inDevice;
		m_pool = inPool;
		m_queue = inQueue;

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = VulkanRHI::get()->getCopyCommandPool();
		allocInfo.commandBufferCount = 1;
		vkAllocateCommandBuffers(*VulkanRHI::get()->getVulkanDevice(),&allocInfo,&cmdBuf);

		fence = VulkanRHI::get()->getFencePool().createFence(false);
	}

	void beginCmd()
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmdBuf,&beginInfo);
	}

	void endCmdAndSubmit()
	{
		vkEndCommandBuffer(cmdBuf);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		vkQueueSubmit(m_queue,1,&submitInfo,fence->getFence());
	}

	// NOTE: Call every frame.
	bool tick()
	{
		if(!m_ready)
		{
			if(VulkanRHI::get()->getFencePool().isFenceSignaled(fence))
			{
				m_ready = true;
				if(finishCallBack)
				{
					finishCallBack();
				}
				release();
			}
		}
		return m_ready;
	}
};

void AssetSystem::checkTextureUploadStateAsync()
{
	for(auto iterP = m_uploadingTextureAsyncTask.begin(); iterP!=m_uploadingTextureAsyncTask.end();)
	{
		bool tickResult = (*iterP)->tick();
		if(tickResult) // finish
		{
			iterP = m_uploadingTextureAsyncTask.erase(iterP);
		}
		else
		{
			++iterP;
		}
	}
}

bool AssetSystem::loadTexture2DImage(CombineTexture& inout,const std::string& gameName)
{
	using namespace asset_system;
	if(std::filesystem::exists(gameName)) // 文件若不存在则返回fasle并换成替代纹理
	{
		CHECK(inout.texture == nullptr);

		AssetFile asset {};
		loadBinFile(gameName.c_str(),asset);
		CHECK(asset.type[0]=='T'&&
			  asset.type[1]=='E'&&
			  asset.type[2]=='X'&&
			  asset.type[3]=='I');

		TextureInfo info = readTextureInfo(&asset);

		// Unpack data to container.
		std::vector<uint8> pixelData{};
		pixelData.resize(info.textureSize);
		unpackTexture(&info,asset.binBlob.data(),asset.binBlob.size(),(char*)pixelData.data());

		inout.sampler = toVkSampler(info.samplerType);

		if(info.bCacheMipmaps)
		{
			inout.texture = Texture2DImage::create(
				VulkanRHI::get()->getVulkanDevice(),
				info.pixelSize[0],
				info.pixelSize[1],
				VK_IMAGE_ASPECT_COLOR_BIT,
				info.getVkFormat(),
				false,
				info.mipmapLevels
			);

			uint32 mipWidth = info.pixelSize[0];
			uint32 mipHeight = info.pixelSize[1];
			uint32 offsetPtr = 0; 

			auto uploader = std::make_shared<GpuUploadTextureAsync>();
			m_uploadingTextureAsyncTask.push_back(uploader);
			uploader->init(VulkanRHI::get()->getCopyCommandPool(),*VulkanRHI::get()->getVulkanDevice(),VulkanRHI::get()->getCopyQueue());

			// 首先填充stageBuffer
			for(uint32 level = 0; level<info.mipmapLevels; level++)
			{
				CHECK(mipWidth >= 1 && mipHeight >= 1);
				uint32 currentMipLevelSize = mipWidth * mipHeight * TEXTURE_COMPONENT;
				auto* stageBuffer = VulkanBuffer::create(
					VulkanRHI::get()->getVulkanDevice(),
					VulkanRHI::get()->getCopyCommandPool(),
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
					VMA_MEMORY_USAGE_CPU_TO_GPU,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					(VkDeviceSize)currentMipLevelSize,
					(void*)(pixelData.data() + offsetPtr)
				);
				uploader->uploadBuffers.push_back(stageBuffer);
				mipWidth  /= 2;
				mipHeight /= 2;
				offsetPtr += currentMipLevelSize;
			}

			const uint32 mipLevels = info.mipmapLevels;
			mipWidth = info.pixelSize[0]; mipHeight = info.pixelSize[1];
			VulkanImage* imageProcess = inout.texture;

			uploader->beginCmd();
			for(uint32 level = 0; level < mipLevels; level++)
			{
				CHECK(mipWidth >= 1 && mipHeight >= 1);
				imageProcess->copyBufferToImage(uploader->cmdBuf,*uploader->uploadBuffers[level],mipWidth,mipHeight,VK_IMAGE_ASPECT_COLOR_BIT,level);
				if(level==(mipLevels-1))
				{
					imageProcess->transitionLayout(uploader->cmdBuf,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_ASPECT_COLOR_BIT);
				}
				mipWidth  /= 2;
				mipHeight /= 2;
			}
			uploader->endCmdAndSubmit();

			uploader->finishCallBack = [&inout]()
			{
				inout.bReady = true;
			};
		}
		else
		{
			LOG_GRAPHICS_INFO("Uploading texture {0} to GPU and generate mipmaps...",gameName);
			inout.texture = Texture2DImage::createAndUpload(
				VulkanRHI::get()->getVulkanDevice(),
				VulkanRHI::get()->getGraphicsCommandPool(),
				VulkanRHI::get()->getGraphicsQueue(),
				pixelData,
				info.pixelSize[0],
				info.pixelSize[1],
				VK_IMAGE_ASPECT_COLOR_BIT,
				info.getVkFormat()
			);

			inout.bReady = true;
		}
		return true;
	}

	return false;
}

// NOTE: 扫描项目文件夹里的资源
//       建立meta资源索引
void AssetSystem::processProjectDirectory(bool firstInit)
{
	AssetCache::s_cache->clear();
	g_assetFolderDirty = true;
	namespace fs = std::filesystem;
	const std::string projectPath = s_projectDir;
	const std::string engineMeshPath = s_engineMesh;

	auto processFile = [&](const std::filesystem::directory_entry& entry)
	{
		if(!fs::is_directory(entry.path()))
		{
			std::string pathStr = FileSystem::toCommonPath(entry);
			std::string suffixStr = FileSystem::getFileSuffixName(pathStr);
			std::string rawName = FileSystem::getFileRawName(pathStr);
			std::string fileName = FileSystem::getFileNameWithoutSuffix(pathStr);

			if(firstInit)
			{
				if(suffixStr.find(".tga")!=std::string::npos || suffixStr.find(".psd")!=std::string::npos)
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
			
			// TODO: 如何实现一个好的资源流送管理工具？
			//       我们的资源上传到GPU后，是否需要引用计数管理其生命周期？
			//       是否应该为引擎加载所有的纹理？
			if(suffixStr.find(".texture") != std::string::npos)
			{
				AssetCache::s_cache->m_texture.emplace(pathStr);
			}
			else if(suffixStr.find(".mesh") != std::string::npos)
			{
				AssetCache::s_cache->m_staticMesh.emplace(pathStr);
			}
			else if(suffixStr.find(".material") != std::string::npos)
			{
				AssetCache::s_cache->m_materials.emplace(pathStr);
			}
		}
	};

	for(const auto& entry:fs::recursive_directory_iterator(projectPath))
	{
		processFile(entry);
	}

	for(const auto& entry:fs::recursive_directory_iterator(engineMeshPath))
	{
		processFile(entry);
	}
}

void AssetSystem::addAsset(std::string path,EAssetFormat format)
{
	switch(format)
	{
	case engine::asset_system::EAssetFormat::T_R8G8B8A8:
	{
		LOG_IO_INFO("Bakeing texture {0}....",path);
		std::string bakeName = rawPathToAssetPath(path,format);
		bool bNeedSrgb = false;
		if(path.find("_Albedo")!=std::string::npos    ||
		   path.find("_Emissive")!=std::string::npos  ||
		   path.find("_BaseColor")!=std::string::npos
			)
		{
			bNeedSrgb = true;
		}
		bakeTexture(path.c_str(),bakeName.c_str(),bNeedSrgb,true,4,true);
		LOG_IO_INFO("Baked texture {0}.",bakeName);
		remove(path.c_str());
		g_assetFolderDirty = true;
		return;
	}
	case engine::asset_system::EAssetFormat::M_StaticMesh_Obj:
	{
		LOG_IO_INFO("Baking static mesh {0}...",path);
		std::string bakeName = rawPathToAssetPath(path,format);
		bakeAssimpMesh(path.c_str(),bakeName.c_str(),true);
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
	whiteTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
	whiteTexture.texture = loadFromFile(s_defaultWhiteTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
	whiteTexture.bReady = true;

	textureLibrary->m_textureContainer[s_defaultBlackTextureName] = {};
	auto& blackTexture = textureLibrary->m_textureContainer[s_defaultBlackTextureName];
	blackTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
	blackTexture.texture = loadFromFile(s_defaultBlackTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
	blackTexture.bReady = true;

	textureLibrary->m_textureContainer[s_defaultNormalTextureName] = {};
	auto& normalTexture = textureLibrary->m_textureContainer[s_defaultNormalTextureName];
	normalTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
	normalTexture.texture = loadFromFile(s_defaultNormalTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
	normalTexture.bReady = true;

	textureLibrary->m_textureContainer[s_defaultCheckboardTextureName] = {};
	auto& checkboxTexture = textureLibrary->m_textureContainer[s_defaultCheckboardTextureName];
	checkboxTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
	checkboxTexture.texture = loadFromFile(s_defaultCheckboardTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);
	checkboxTexture.bReady = true;

	textureLibrary->m_textureContainer[s_defaultEmissiveTextureName] = {};
	auto& emissiveTexture = textureLibrary->m_textureContainer[s_defaultEmissiveTextureName];
	emissiveTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
	emissiveTexture.texture = loadFromFile(s_defaultEmissiveTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);
	emissiveTexture.bReady = true;
}

void AssetCache::clear()
{
	m_staticMesh.clear();
	m_texture.clear();
	m_materials.clear();
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