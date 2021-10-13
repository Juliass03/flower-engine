#include "texture.h"
#include "../launch/launch_engine_loop.h"
#include "../asset_system/asset_system.h"
#include "../core/file_system.h"

using namespace engine;

TextureLibrary* TextureLibrary::s_textureLibrary = new TextureLibrary();

bool engine::TextureLibrary::existTexture(const std::string& name)
{
	return m_textureContainer.find(name) != m_textureContainer.end();
}

std::pair<ERequestTextureResult /*use temp texture */,CombineTexture&> engine::TextureLibrary::getCombineTextureByName(const std::string& gameName)
{
	if(FileSystem::endWith(gameName,".tga") && TextureLibrary::get()->existTexture(gameName))
	{
		return { ERequestTextureResult::Ready, m_textureContainer[gameName]};
	}

	if(asset_system::AssetCache::s_cache->m_texture.find(gameName) == asset_system::AssetCache::s_cache->m_texture.end())
	{
		LOG_WARN("Texture {0} no exists! Will replace with default white texture!",gameName);
		return { ERequestTextureResult::NoExist, m_textureContainer[s_defaultWhiteTextureName]};
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
}
