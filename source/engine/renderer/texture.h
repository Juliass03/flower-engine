#pragma once
#include "../core/core.h"
#include "../vk/vk_rhi.h"

namespace engine{

namespace asset_system
{
	class AssetSystem;
}

struct CombineTexture
{
	Texture2DImage* texture = nullptr;
	VkSampler sampler;

	bool bReady = false;
};

enum class ERequestTextureResult
{
	Ready,
	Loading,
	NoExist,
};

// Init in AssetSystem::loadEngineTextures()
// TODO: 增加引用计数，在没有引用且显存占用达到一定阈值时销毁Texture
class TextureLibrary
{
	using TextureContainer = std::unordered_map<std::string,CombineTexture>;
	friend asset_system::AssetSystem;
private:
	Ref<asset_system::AssetSystem> m_assetSystem = nullptr;
	static TextureLibrary* s_textureLibrary;
	TextureContainer m_textureContainer;

public:
	bool existTexture(const std::string& name);
	std::pair<ERequestTextureResult,CombineTexture&> getCombineTextureByName(const std::string& gameName);
	bool textureReady(const std::string& name);
	bool textureLoading(const std::string& name);
	void release();
	static TextureLibrary* get()
	{
		return s_textureLibrary;
	}
};

}