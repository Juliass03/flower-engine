#pragma once
#include <string>
#include <vector>
#include "../vk/vk_rhi.h"
#include "../core/file_system.h"
#include "../core/core.h"
#include "../core/runtime_module.h"
#include "shader_effect.h"
#include <bitset>

namespace engine{ namespace shaderCompiler{

extern bool g_globalPassShouldRebuild;
extern bool g_meshPassShouldRebuild;

enum class EShaderPass
{
	GBuffer,
	Tonemapper,
	Lighting,
	ShadowDepth,
	Unknow,
	Max,
};

namespace fallbackShaderName
{
	static const char* gbuffer = "Fallback:GBuffer";
	static const char* depth = "Fallback:Depth";
}

namespace fallbackShaderPath
{
	static const char* gbufferVert = "media/shader/fallback/bin/gbuffer.vert.spv";
	static const char* gbufferFrag = "media/shader/fallback/bin/gbuffer.frag.spv";

	static const char* tonemapperVert = "media/shader/fallback/bin/tonemapper.vert.spv";
	static const char* tonemapperFrag = "media/shader/fallback/bin/tonemapper.frag.spv";

	static const char* lightingVert = "media/shader/fallback/bin/lighting.vert.spv";
	static const char* lightingFrag = "media/shader/fallback/bin/lighting.frag.spv";

	static const char* depthVert = "media/shader/fallback/bin/depth.vert.spv";
	static const char* depthFrag = "media/shader/fallback/bin/depth.frag.spv";
}

static std::string toString(EShaderPass passType)
{
	if(passType == EShaderPass::GBuffer)
	{
		return "GBuffer";
	}
	else if(passType == EShaderPass::Tonemapper)
	{
		return "Tonemapper";
	}
	else if(passType == EShaderPass::Lighting)
	{
		return "Lighting";
	}
	else if(passType == EShaderPass::ShadowDepth)
	{
		return "ShadowDepth";
	}
	else
	{
		return "Unknow";
	}
}

static EShaderPass toShaderPassType(const std::string& name)
{
	if(name=="GBuffer")
	{
		return EShaderPass::GBuffer;
	}
	else if(name=="Tonemapper")
	{
		return EShaderPass::Tonemapper;
	}
	else if(name=="ShadowDepth")
	{
		return EShaderPass::ShadowDepth;
	}
	else if(name=="Lighting")
	{
		return EShaderPass::Lighting;
	}
	else
	{
		return EShaderPass::Unknow;
	}
}

struct ShaderInfo
{
	EShaderPass passType;

	std::string vertShaderPath;
	std::string fragShaderPath;

	std::string compiled_vertShaderPath;
	std::string compiled_fragShaderPath;

	bool compiled = false; // 已经编译过了？
	bool compiling = false; // 编译中？
	bool compiled_suceess = false; // 编译的是否成功？

	void compileShader();
	ShaderInfo();

	std::shared_ptr<ShaderEffect> effect;

	std::string shaderName;
	std::string lastEditTime;
};

extern bool compileShader(const std::string& path,ShaderInfo& inout);

struct ShaderAction
{
	enum Action
	{
		Add,
		Remove,
		Modified,
	};

	ShaderInfo info;
	Action action;
};

struct ShaderCompact
{
	Ref<VulkanShaderModule> vertex;
	Ref<VulkanShaderModule> frag;
};

class ShaderCompiler : public IRuntimeModule
{
public:
	virtual bool init() override;
	virtual void tick(float dt) override;
	virtual void release() override;

private:
	FileWatcher m_engineShadersWatcher;
	std::mutex mtx; 

	// 缓存shader名的哈希表
	std::unordered_map<std::string/* shader name */,std::shared_ptr<ShaderInfo>/* shader info */> m_cacheShaderMap;

	// 监视线程写
	// 主线程读 + 清理。 
	std::unordered_map<std::string/* shader file path */,ShaderAction/* shader info */> m_dirtyShaderMaps;

	bool containShader(const std::string& key)
	{
		auto el = m_cacheShaderMap.find(key);
		return el != m_cacheShaderMap.end();
	}

private:
	void buildCallbackShader();


private:
	// 主线程的flush tick
	void flushDirtyShaderMap();

	const std::unordered_map<std::string/* shader name */,std::shared_ptr<ShaderInfo>/* shader info */>& getCacheShader() const
	{
		return m_cacheShaderMap;
	}

	void shaderFileWatch(std::string path_to_watch, engine::FileWatcher::FileStatus status,std::filesystem::file_time_type);

	ShaderCompact getFallbackShader(EShaderPass passType);

public:
	ShaderCompiler(Ref<ModuleManager>);

	ShaderCompact getShader(const std::string& shaderName,EShaderPass passType);

	std::shared_ptr<ShaderInfo> getMeshPassShaderInfo(const std::string& shaderName,EShaderPass passType);
};

}}