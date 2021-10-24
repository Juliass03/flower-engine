#pragma once
#include <string>
#include <vector>
#include "../vk/vk_rhi.h"
#include "../core/file_system.h"
#include "../core/core.h"
#include "../core/runtime_module.h"

namespace engine{ namespace shaderCompiler{

extern bool g_shaderPassChange;

enum class EShaderPass
{
	GBuffer,
	Depth,
	Tonemapper,
	Lighting,

	Unknow,
	Max,
};

namespace fallbackShaderName
{
	static const char* gbuffer    = "Fallback:GBuffer";
	static const char* depth      = "Fallback:Depth";
	static const char* lighting   = "Fallback:Lighting";
	static const char* tonemapper = "Fallback:Tonemapper";
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

extern std::string toString(EShaderPass passType);
extern EShaderPass toShaderPassType(const std::string& name);

struct ShaderInfo
{
	EShaderPass passType = EShaderPass::Unknow;

	std::string shaderName;
	std::string lastEditTime;

	std::string vertShaderPath;
	std::string fragShaderPath;

	std::string compiled_vertShaderPath;
	std::string compiled_fragShaderPath;

	bool compiled         = false; // 已经编译过了？
	bool compiling        = false; // 编译中？
	bool compiled_suceess = false; // 编译的是否成功？

	void compileShader();
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
	
	// NOTE: 缓存shader名的哈希表
	std::unordered_map<std::string/*shader name*/,std::shared_ptr<ShaderInfo>/*shader info*/> m_cacheShaderMap;

	// NOTE: 监视线程写
	//       主线程读 + 清理。 
	std::unordered_map<std::string/*shader file path*/,ShaderAction/*shader info*/> m_dirtyShaderMaps;
	std::mutex m_dirtyShaderMapMutex; 

private:
	void flushDirtyShaderMap();
	void shaderFileWatch(std::string path_to_watch, engine::FileWatcher::FileStatus status,std::filesystem::file_time_type);
	ShaderCompact getFallbackShader(EShaderPass passType);
	const std::unordered_map<std::string/*shader name*/,std::shared_ptr<ShaderInfo>/*shader info*/>& getCacheShader() const;
	bool containShader(const std::string& key) const;

public:
	ShaderCompiler(Ref<ModuleManager>);
	ShaderCompact getShader(const std::string& shaderName,EShaderPass passType);
};

}}