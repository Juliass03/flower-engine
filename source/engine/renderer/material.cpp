#include "material.h"
#include "mesh.h"
#include "texture.h"
#include "renderer.h"
#include <filesystem>
#include <future>
#include "../core/file_system.h"
#include "frame_data.h"

using namespace engine;

MaterialLibrary* engine::MaterialLibrary::s_materialLibrary = new MaterialLibrary();

MaterialLibrary::MaterialLibrary()
{
	m_callBackMaterial.baseColorTexture = s_defaultCheckboardTextureName;
	m_callBackMaterial.emissiveTexture = s_defaultEmissiveTextureName;
	m_callBackMaterial.normalTexture = s_defaultNormalTextureName;
	m_callBackMaterial.specularTexture = s_defaultBlackTextureName;
}

engine::MaterialLibrary::~MaterialLibrary()
{
	for(auto& pair : m_materialContainer)
	{
		delete pair.second;
	}
	m_materialContainer.clear();
}

std::unordered_map<std::string,Material*>& engine::MaterialLibrary::getContainer()
{
	return m_materialContainer;
}

MaterialLibrary* engine::MaterialLibrary::get()
{
	return s_materialLibrary;
}

bool engine::MaterialLibrary::existMaterial(const std::string& name)
{
	return m_materialContainer.find(name) != m_materialContainer.end();
}

Material* engine::MaterialLibrary::createEmptyMaterialAsset(const std::string& name)
{
	if(existMaterial(name))
	{
		return m_materialContainer[name];
	}

	auto* newMat = new Material();
	m_materialContainer[name] = newMat;

	std::ofstream os(name);
	cereal::JSONOutputArchive archive(os);
	archive(*newMat);

	return newMat;
}

Material* engine::MaterialLibrary::getMaterial(const std::string& name)
{
	if(existMaterial(name))
	{
		return m_materialContainer[name];
	}

	auto newMat = new Material();
	m_materialContainer[name] = newMat;

	std::ifstream os(name);
	cereal::JSONInputArchive iarchive(os);
	iarchive(*newMat);

	return newMat;
}

Material& engine::MaterialLibrary::getCallbackMaterial()
{
	return m_callBackMaterial;
}

GPUMaterialData engine::Material::getGPUMaterialData()
{
	if(bSomeTextureLoading)
	{
		bool bLoading = false;

		auto baseColorData = TextureLibrary::get()->getCombineTextureByName(baseColorTexture);
		m_cacheGPUMaterialData.baseColorTexId = baseColorData.second.bindingIndex;
		bLoading = bLoading || ( baseColorData.first == ERequestTextureResult::Loading );

		auto normalData = TextureLibrary::get()->getCombineTextureByName(normalTexture);
		m_cacheGPUMaterialData.normalTexId = normalData.second.bindingIndex;
		bLoading = bLoading || ( normalData.first == ERequestTextureResult::Loading );

		auto specularData = TextureLibrary::get()->getCombineTextureByName(specularTexture);
		m_cacheGPUMaterialData.specularTexId = specularData.second.bindingIndex;
		bLoading = bLoading || ( specularData.first == ERequestTextureResult::Loading );

		auto emissiveData = TextureLibrary::get()->getCombineTextureByName(emissiveTexture);
		m_cacheGPUMaterialData.emissiveTexId = emissiveData.second.bindingIndex;
		bLoading = bLoading || ( emissiveData.first == ERequestTextureResult::Loading );

		bSomeTextureLoading = bLoading;
	}

	return m_cacheGPUMaterialData;
}
