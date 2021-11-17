#include "texture.h"
#include "../launch/launch_engine_loop.h"
#include "../asset_system/asset_system.h"
#include "../core/file_system.h"

using namespace engine;
TextureLibrary* TextureLibrary::s_textureLibrary = new TextureLibrary();

void engine::TextureLibrary::createBindlessTextureDescriptorHeap()
{
	VkDescriptorSetLayoutBinding binding{};
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	binding.binding = 0;
	binding.descriptorCount = VulkanRHI::get()->getPhysicalDeviceDescriptorIndexingProperties().maxDescriptorSetUpdateAfterBindSampledImages;
	
	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.bindingCount = 1;
	setLayoutCreateInfo.pBindings = &binding;
	setLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

	const VkDescriptorBindingFlagsEXT flags =
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags{};
	bindingFlags.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	bindingFlags.bindingCount   = 1;
	bindingFlags.pBindingFlags  = &flags;
	setLayoutCreateInfo.pNext = &bindingFlags;

	vkCheck(vkCreateDescriptorSetLayout(
		VulkanRHI::get()->getDevice(), 
		&setLayoutCreateInfo, 
		nullptr,
		&m_bindlessTextureDescriptorHeap.setLayout));

	VkDescriptorPoolSize  poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = MAX_GPU_LOAD_TEXTURE_COUNT;

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes    = &poolSize;
	poolCreateInfo.maxSets       = 1;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

	vkCheck(vkCreateDescriptorPool(*VulkanRHI::get()->getVulkanDevice(), &poolCreateInfo, nullptr, &m_bindlessTextureDescriptorHeap.descriptorPool));

	VkDescriptorSetAllocateInfo allocateInfo {};
	allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool     = m_bindlessTextureDescriptorHeap.descriptorPool;
	allocateInfo.pSetLayouts        = &m_bindlessTextureDescriptorHeap.setLayout;
	allocateInfo.descriptorSetCount = 1;
	
	VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableInfo{};
	variableInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	variableInfo.descriptorSetCount = 1;
	allocateInfo.pNext              = &variableInfo;

	const uint32 NumDescriptorsTextures = MAX_GPU_LOAD_TEXTURE_COUNT;
	variableInfo.pDescriptorCounts = &NumDescriptorsTextures;
	vkCheck(vkAllocateDescriptorSets(*VulkanRHI::get()->getVulkanDevice(), &allocateInfo, &m_bindlessTextureDescriptorHeap.descriptorSetUpdateAfterBind));
}

bool engine::TextureLibrary::existTexture(const std::string& name)
{
	return m_textureContainer.find(name) != m_textureContainer.end();
}

std::pair<ERequestTextureResult /*use temp texture */,CombineTexture&> engine::TextureLibrary::getCombineTextureByName(const std::string& gameName)
{
	
	if(!std::filesystem::exists(gameName))
	{
		LOG_WARN("Texture {0} no exists! Will replace with default white texture!",gameName);
		return { ERequestTextureResult::NoExist, m_textureContainer[s_defaultWhiteTextureName]};
	}

	// ÒýÇæÎÆÀí
	if(FileSystem::endWith(gameName,".tga") && TextureLibrary::get()->existTexture(gameName))
	{
		return { ERequestTextureResult::Ready, m_textureContainer[gameName]};
	}

	if(!m_assetSystem)
	{
		m_assetSystem = g_engineLoop.getEngine()->getRuntimeModule<asset_system::AssetSystem>();
	}

	if(m_textureContainer.find(gameName) != m_textureContainer.end() && m_textureContainer[gameName].bReady)
	{
		return { ERequestTextureResult::Ready,m_textureContainer[gameName]};
	}

	m_assetSystem->addLoadTextureTask(gameName);
	return { ERequestTextureResult::Loading,m_textureContainer[s_defaultCheckboardTextureName]};
}

bool engine::TextureLibrary::textureReady(const std::string& name)
{
	if(m_textureContainer.find(name) != m_textureContainer.end() && m_textureContainer[name].bReady)
	{
		return true;
	}

	return false;
}

bool engine::TextureLibrary::textureLoading(const std::string& name)
{
	if(m_textureContainer.find(name) != m_textureContainer.end() && !m_textureContainer[name].bReady)
	{
		return true;
	}

	return false;
}

void engine::TextureLibrary::init()
{
	createBindlessTextureDescriptorHeap();
}

void engine::TextureLibrary::release()
{
	for(auto& texturePair : m_textureContainer)
	{
		if(texturePair.second.texture) // ´Ë´¦ÅÐ¿Õ·ÀÖ¹ÖØ¸´É¾³ý
		{
			texturePair.second.texture->release();
			delete texturePair.second.texture;
			texturePair.second.texture = nullptr;
		}
	}

	m_textureContainer.clear();

	VkDevice device = *VulkanRHI::get()->getVulkanDevice();
	vkDestroyDescriptorSetLayout(device, m_bindlessTextureDescriptorHeap.setLayout, nullptr);
	vkDestroyDescriptorPool(device, m_bindlessTextureDescriptorHeap.descriptorPool, nullptr);
}

VkDescriptorSet engine::TextureLibrary::getBindlessTextureDescriptorSet()
{
	return m_bindlessTextureDescriptorHeap.descriptorSetUpdateAfterBind;
}

VkDescriptorSetLayout engine::TextureLibrary::getBindlessTextureDescriptorSetLayout()
{
	return m_bindlessTextureDescriptorHeap.setLayout;
}

void engine::TextureLibrary::updateTextureToBindlessDescriptorSet(CombineTexture& inout)
{
	CHECK(inout.bReady);

	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = inout.sampler;
	imageInfo.imageView = inout.texture->getImageView();
	imageInfo.imageLayout = inout.texture->getCurentLayout();

	VkWriteDescriptorSet  write{};
	write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet          = getBindlessTextureDescriptorSet();
	write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.dstBinding      = 0;
	write.pImageInfo      = &imageInfo;
	write.descriptorCount = 1;
	write.dstArrayElement = getBindlessTextureCountAndAndOne();

	vkUpdateDescriptorSets(VulkanRHI::get()->getDevice(), 1, &write, 0, nullptr);
	inout.bindingIndex = write.dstArrayElement;
}

uint32 engine::TextureLibrary::getBindlessTextureCountAndAndOne()
{
	uint32 index = 0;
	m_bindlessTextureCountLock.lock();
	{
		index = m_bindlessTextureCount;
		m_bindlessTextureCount ++;
		CHECK(m_bindlessTextureCount < MAX_GPU_LOAD_TEXTURE_COUNT);
	}
	m_bindlessTextureCountLock.unlock();

	return index;
}