#pragma once

#include "../core/core.h"
#include "../vk/vk_rhi.h"
#include "frame_data.h"
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp> 
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/base_class.hpp>
#include "../shader_compiler/shader_compiler.h"
#include <utility>
#include <cereal/types/utility.hpp>

namespace engine{

// NOTE: 2021/10/24 12:01
//       引擎已经升级到了Bindless式架构
//       不再在一个Renderpass中为多个网格使用多种Shader，而是一个Renderpass使用一个固定的Uber Shader
struct Material
{
	std::string baseColorTexture;
	std::string normalTexture;
	std::string specularTexture;
	std::string emissiveTexture;

	template <class Archive> void serialize(Archive& ar)
	{
		ar( baseColorTexture
		   ,normalTexture
		   ,specularTexture
		   ,emissiveTexture);
	}

	GPUMaterialData getGPUMaterialData();

private:
	bool bSomeTextureLoading = true;
	GPUMaterialData m_cacheGPUMaterialData;
};

class MaterialLibrary
{
private:
	// TODO: 换成Memory Allocation + Placement New的做法
	std::unordered_map<std::string/*file path*/,Material*> m_materialContainer;
	static MaterialLibrary* s_materialLibrary;
	Material m_callBackMaterial;
	MaterialLibrary();
	~MaterialLibrary();

public:
	std::unordered_map<std::string,Material*>& getContainer();
	static MaterialLibrary* get();

	bool existMaterial(const std::string& name);
	Material* createEmptyMaterialAsset(const std::string& name);
	Material* getMaterial(const std::string& name);

	Material& getCallbackMaterial();
};

}