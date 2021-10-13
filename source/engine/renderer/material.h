#pragma once

#include "../core/core.h"
#include "../vk/vk_rhi.h"

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

constexpr auto MAIN_TEXTURE_NAME = "BaseColorTexture";

// NOTE: 编辑器中根据PassType选择ShaderName
//       然后在Material中透过constructMaterial函数构建反射信息
//       材质编辑器中再修改

// 可序列化的资产
struct MaterialShaderInfo
{
	// NOTE: 编辑器将会从可用的材质集中选择对应的Shader
	std::string shaderName = "";
	shaderCompiler::EShaderPass passType;

	// NOTE: 材质反射信息
	std::unordered_map<std::string /*shader texture name*/,
	                   std::pair<uint32 /* shader binding */,
					   std::string /*resource texture name*/>> textures {};

	template <class Archive>
	void load(Archive& ar)
	{
		int type = 0;
		ar( 
			cereal::make_nvp("Name", shaderName),
			cereal::make_nvp("Textures", textures),
			cereal::make_nvp("PassType", type)
		);
		passType = static_cast<shaderCompiler::EShaderPass>(type);
	}

	template <class Archive>
	void save(Archive& ar) const
	{
		const auto type = static_cast<int>(passType);
		ar( 
			cereal::make_nvp("Name", shaderName),
			cereal::make_nvp("Textures", textures),
			cereal::make_nvp("PassType", type)
		);
	}

	MaterialShaderInfo() { }
	MaterialShaderInfo(const std::string& name,shaderCompiler::EShaderPass type);
	
	static bool createEmptyMaterialAsset(const std::string& path);
};

struct MeshPassLayout;

class Material
{
	struct DescriptorSetCache
	{
		VulkanDescriptorSetReference set;
		bool bRebuild = true;
	};

public:
	std::shared_ptr<MaterialShaderInfo> m_materialShaderInfo = nullptr;

	// NOTE：根据编辑器中选择的材质名称构建材质函数并提取反射数据
	void constructMaterial(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,shaderCompiler::EShaderPass type);
	
	void release();

	void getMaterialDescriptorSet(const MeshPassLayout& pass_layout,bool bRebuildMaterial);
	std::shared_ptr<shaderCompiler::ShaderInfo> getShaderInfo() { return m_shaderInfo; }
	bool allShaderInfoReady();
	
private:
	std::shared_ptr<shaderCompiler::ShaderInfo> m_shaderInfo = nullptr;
	std::shared_ptr<shaderCompiler::ShaderInfo> m_depthShaderInfo = nullptr;

	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
	DescriptorSetCache m_gbufferDescriptorSet; // ref
	DescriptorSetCache m_depthDesctiptorSet; // ref
	bool m_hasCustomDescriptor = false;
	
	VkPipeline m_gbufferPipeline = VK_NULL_HANDLE;// self
	VkPipeline m_depthPipeline = VK_NULL_HANDLE; // setlf
	std::vector<VkPipeline> m_outOfDayPipeline{};

	bool m_lastTimeUseTempTexture = false;
	void buildMeshDescriptorSet(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,const MeshPassLayout& pass_layout,bool bRebuildMaterial);

	void releaseGbufferResource();
	void releaseDepthResource();
public:
	VkPipeline getGbufferPipeline() { return m_gbufferPipeline; }
	VkPipeline getDepthPipeline() { return m_depthPipeline; }
	void bindGbuffer(VkCommandBuffer cmd);
	void bindDepth(VkCommandBuffer cmd); 

	friend class MaterialLibrary;
};

struct MaterialInfoCache
{
	std::unordered_map<std::string,std::shared_ptr<MaterialShaderInfo>> cacheInfos;
	static MaterialInfoCache* s_cache;

	std::shared_ptr<MaterialShaderInfo> getMaterialInfoCache(const std::string&);
};

class MaterialLibrary
{
private:
	// NOTE: 通过名字来缓存材质
	std::unordered_map<std::string,Material*> m_materialContainer;
	bool bInit = false;
	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;

public:
	void init(Ref<shaderCompiler::ShaderCompiler> shaderCompiler);
	Ref<Material> registerMaterial(Ref<shaderCompiler::ShaderCompiler> shaderCompiler,const std::string& shaderName);
	bool isInit() const { return bInit; };
	void release();
	std::unordered_map<std::string,Material*>& getContainer() { return m_materialContainer; }

	Ref<Material> getFallback(const MeshPassLayout& layout,shaderCompiler::EShaderPass type);
};

}