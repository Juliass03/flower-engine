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

namespace engine{ namespace asset_system{

EngineAsset* EngineAsset::s_asset = new EngineAsset();

AssetSystem::AssetSystem(Ref<ModuleManager> in): IRuntimeModule(in) {  }

bool AssetSystem::init()
{
	loadEngineTextures();
	processProjectDirectory();
	return true;
}

void AssetSystem::tick(float dt)
{
	// 隔6帧扫描一次
	constexpr int32 scanTime = 1000;
	static int32 scanTick = 0;
	int32 fps = static_cast<int32>(getFps());
	fps = fps > 0 ? fps : scanTime;
	if(scanTick >= fps * 6)
	{
		scanTick = 0;
		processProjectDirectory();
	}
	else
	{
		scanTick ++;
	}

	prepareTextureLoad();
	checkTextureUploadStateAsync();

	broadcastCallbackOnAssetFolderDirty();
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

void AssetSystem::prepareTextureLoad()
{
	for(auto& path : m_perScanAdditionalTextures)
	{
		m_loadingTextureTasks.push(path);
	}
	m_perScanAdditionalTextures.resize(0);

	// NOTE: 我们现在引入了异步上传纹理的队列，因此可以在单独的线程中加载了。
	constexpr auto perTickLoadNum = 100;
	for(auto i = 0; i < perTickLoadNum; i++)
	{
		if(m_loadingTextureTasks.size()==0)
		{
			break;
		}

		const auto& path = m_loadingTextureTasks.front();
		auto& textureContainer = TextureLibrary::s_textureLibrary->m_textureContainer;
		CHECK(textureContainer.find(path) == textureContainer.end());
		textureContainer[path] = {};
		loadTexture2DImage(textureContainer[path],path);
		m_loadingTextureTasks.pop();
	}
}

// NOTE: 扫描项目文件夹里的资源
//       建立meta资源索引
void AssetSystem::processProjectDirectory()
{
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

			// 原始资源转换为引擎资源
			if(suffixStr.find(".tga")!=std::string::npos || suffixStr.find(".psd") != std::string::npos)
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

			if(suffixStr.find(".texture") != std::string::npos)
			{
				
			}
			else if(suffixStr.find(".mesh") != std::string::npos)
			{
				MeshLibrary::get()->emplaceStaticeMeshList(pathStr);
			}
			else if(suffixStr.find(".material") != std::string::npos)
			{
				
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
		if(path.find("_Albedo.")!=std::string::npos    ||
		   path.find("_Emissive.")!=std::string::npos  ||
		   path.find("_BaseColor.")!=std::string::npos
			)
		{
			bNeedSrgb = true;
		}
		bakeTexture(path.c_str(),bakeName.c_str(),bNeedSrgb,true,4,true,true,EBlockType::BC3);
		LOG_IO_INFO("Baked texture {0}.",bakeName);
		remove(path.c_str());

		// NOTE: 通知UI面板扫描
		m_bAssetFolderDirty = true;
		return;
	}
	case engine::asset_system::EAssetFormat::M_StaticMesh_Obj:
	{
		LOG_IO_INFO("Baking static mesh {0}...",path);
		std::string bakeName = rawPathToAssetPath(path,format);
		bakeAssimpMesh(path.c_str(),bakeName.c_str(),true);
		LOG_IO_INFO("Baked static mesh {0}.",bakeName);
		remove(path.c_str());

		// NOTE: 通知UI面板扫描
		m_bAssetFolderDirty = true;
		return;
	}
	case engine::asset_system::EAssetFormat::Unknown:
	default:
		LOG_FATAL("Unkonw asset type!");
		return;
	}
}

void AssetSystem::broadcastCallbackOnAssetFolderDirty()
{
	if(!m_bAssetFolderDirty) return;

	for(auto& pair : m_callbackOnAssetFolderDirty)
	{
		if(pair.second)
		{
			pair.second();
		}
	}

	m_bAssetFolderDirty = false;
}

void AssetSystem::registerOnAssetFolderDirtyCallBack(std::string&& name,std::function<void()>&& callback)
{
	for(auto& pair : m_callbackOnAssetFolderDirty) 
	{
		if(pair.first == name && pair.second) return;
	}

	m_callbackOnAssetFolderDirty.push_back({name,callback});
}

void AssetSystem::unregisterOnAssetFolderDirtyCallBack(std::string&& name)
{
	for(auto& pair : m_callbackOnAssetFolderDirty) 
	{
		if(pair.first == name && pair.second)
		{
			pair.second = nullptr;
		}
	}
}

}}