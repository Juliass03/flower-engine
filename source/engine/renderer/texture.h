#pragma once
#include "../core/core.h"
#include "../vk/vk_rhi.h"

namespace engine{

constexpr auto MAX_GPU_LOAD_TEXTURE_COUNT = 10000;
namespace asset_system
{
	class AssetSystem;
}

class PMXManager;

struct CombineTexture
{
	Texture2DImage* texture = nullptr;
	VkSampler sampler;

	uint32 bindingIndex = 0; // 在bindless descriptor set中的索引
	bool bReady = false;
};

enum class ERequestTextureResult
{
	Ready,
	Loading,
	NoExist,
};

class TextureLibrary
{
	using TextureContainer = std::unordered_map<std::string,CombineTexture>;
	friend asset_system::AssetSystem;
	friend PMXManager;

private:
	Ref<asset_system::AssetSystem> m_assetSystem = nullptr;
	static TextureLibrary* s_textureLibrary;
	TextureContainer m_textureContainer;

	struct BindlessTextureDescriptorHeap
	{
		VkDescriptorSetLayout setLayout{};
		VkDescriptorPool      descriptorPool{};
		VkDescriptorSet       descriptorSetUpdateAfterBind{};
	} m_bindlessTextureDescriptorHeap;

	void createBindlessTextureDescriptorHeap();

	uint32 m_bindlessTextureCount = 0;
	std::mutex m_bindlessTextureCountLock;
	uint32 getBindlessTextureCountAndAndOne();

public:
	bool existTexture(const std::string& name);
	std::pair<ERequestTextureResult,CombineTexture&> getCombineTextureByName(const std::string& gameName);
	bool textureReady(const std::string& name);
	bool textureLoading(const std::string& name);

	void init();
	void release();

	VkDescriptorSet getBindlessTextureDescriptorSet();
	VkDescriptorSetLayout getBindlessTextureDescriptorSetLayout();

	static TextureLibrary* get()
	{
		return s_textureLibrary;
	}

	void updateTextureToBindlessDescriptorSet(CombineTexture& inout);
};

}