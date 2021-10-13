#include "shader_compiler.h"
#include "../core/core.h"
#include "../core/file_system.h"
#include <fstream>
#include <sstream> 
#include <ostream>
#include <optional>
#include <regex>
#include <future>
#include <functional>
#include <chrono>
#include "../renderer/mesh.h"
#include <fstream>
#include "../renderer/material.h"
#include "../renderer/renderer.h"
#include "../launch/launch_engine_loop.h"
#include "../core/job_system.h"

using namespace engine;
using namespace engine::shaderCompiler;

const char* engineShaderTag = ":/";
bool engine::shaderCompiler::g_globalPassShouldRebuild = false;
bool engine::shaderCompiler::g_meshPassShouldRebuild = false;

static AutoCVarInt32 cVarRemoveShaderString(
	"r.ShaderCompiler.RemoveShaderText",
	"Remove shader text when spv has compiled? 0 is off, others are on.",
	"ShaderCompiler",
	1,
	CVarFlags::ReadOnly | CVarFlags::InitOnce
);

template <typename TP>
long long to_time_t(TP tp)
{
	using namespace std::chrono;
	auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
		+ system_clock::now());
	return system_clock::to_time_t(sctp);
}

bool engine::shaderCompiler::compileShader(const std::string& path,ShaderInfo& inout)
{
	std::ifstream infile;
	infile.open(path);

	if (!infile) 
	{
		LOG_ERROR("File path {0} is unvalidation.",path);
		return false;
	}

	std::string folderName = FileSystem::getFolderName(path);
	ShaderInfo& shaderInfo = inout;

	std::string shaderName = "";

	std::string line;
	std::vector<std::string> tokens;

	// 1. token提取
	while(getline(infile,line))
	{
		std::stringstream lineStream(line); 
		std::string token;

		while(getline(lineStream,token,' '))
		{
			if(token.find_first_not_of(' ') != std::string::npos)
			{
				tokens.push_back(token);
				// LOG_INFO(token);
			}
		}
	}
	
	#define CHECK_FORMAT(iter) \
		if(iter == tokens.end()) \
		{\
			LOG_ERROR("Error shader foramt.");\
			return false;\
		}

	// 2. 提取每个Pass信息
	for(auto iter = tokens.begin(); iter != tokens.end(); ++ iter)
	{
		// 走一遍shader名字流程
		if(*iter == "shader")
		{
			if(shaderName!="")
			{
				LOG_ERROR("ERROR shader format.");
				return false;
			}

			iter++; CHECK_FORMAT(iter);

			shaderName = *iter;
			continue;
		}

		// 走一遍Pass处理流程
		if(*iter=="pass")
		{
			iter++; CHECK_FORMAT(iter);
			shaderInfo.passType = toShaderPassType(*iter);

			continue;
		}

		if(*iter == "vert" || *iter == "frag")
		{
			std::string type = *iter;
			iter++; CHECK_FORMAT(iter);

			std::string shaderpathName = *iter;

			if (shaderpathName.rfind(":/", 0) == 0) 
			{ 
				shaderpathName = std::regex_replace(shaderpathName,std::regex(engineShaderTag),s_engineShader);
			}
			else
			{
				shaderpathName = folderName + shaderpathName;
			}

			if(type == "vert") shaderInfo.vertShaderPath = shaderpathName;
			if(type == "frag") shaderInfo.fragShaderPath = shaderpathName;

			continue;
		}
	}

	shaderInfo.shaderName = shaderName;
	infile.close();

	return true;
}

// 获取shader cache name
std::string getShaderCacheName(ShaderInfo info)
{
	std::stringstream ss;

	ss << info.shaderName;
	ss << uint32_t(info.passType);

	ss << info.lastEditTime;
	std::string str = ss.str();

	return std::to_string(fnv1a32(str.c_str(),str.length()));
}

bool engine::shaderCompiler::ShaderCompiler::init()
{
	buildCallbackShader();

	// 初始化时先刷新一遍脏shader
	flushDirtyShaderMap();

	return true;
}

void engine::shaderCompiler::ShaderCompiler::tick(float dt)
{
	flushDirtyShaderMap();
}

void engine::shaderCompiler::ShaderCompiler::release()
{
	m_cacheShaderMap.clear();
	m_engineShadersWatcher.end();
}

void engine::shaderCompiler::ShaderCompiler::flushDirtyShaderMap()
{
	// 直接拷贝一份出来以防止太长时间的监视线程阻塞
	mtx.lock();
	std::unordered_map<std::string,ShaderAction> copyThreadSafeDirtyShaderMaps = m_dirtyShaderMaps;
	m_dirtyShaderMaps.clear();
	mtx.unlock();

	for(auto& pair : copyThreadSafeDirtyShaderMaps)
	{
		const ShaderAction& shader_action = pair.second;

		const bool bShaderHasCache = containShader(pair.second.info.shaderName);
		if(bShaderHasCache) // 已经缓存的
		{
			if(shader_action.action == ShaderAction::Action::Add)
			{
				m_cacheShaderMap[shader_action.info.shaderName] = std::make_shared<ShaderInfo>(shader_action.info);
				m_cacheShaderMap[shader_action.info.shaderName]->compileShader();
			}
			else if(shader_action.action == ShaderAction::Action::Modified)
			{
				// 清理原有的shadercache然后编译新的ShaderCache
				m_cacheShaderMap[shader_action.info.shaderName]->compiled = false;
				m_cacheShaderMap[shader_action.info.shaderName] = std::make_shared<ShaderInfo>(shader_action.info);
				m_cacheShaderMap[shader_action.info.shaderName]->compileShader();
			}
			else if(shader_action.action == ShaderAction::Action::Remove)
			{	
				// 移除键
				m_cacheShaderMap[shader_action.info.shaderName]->compiled = false;
				m_cacheShaderMap[shader_action.info.shaderName]->compiling = false;
				m_cacheShaderMap[shader_action.info.shaderName]->compiled_suceess = false;
				m_cacheShaderMap[shader_action.info.shaderName] = nullptr;
				m_cacheShaderMap.erase(shader_action.info.shaderName);
			}
		}
		else // 未缓存的
		{
			if(shader_action.action == ShaderAction::Action::Add || shader_action.action == ShaderAction::Action::Modified)
			{
				m_cacheShaderMap[shader_action.info.shaderName] = std::make_shared<ShaderInfo>(shader_action.info);
				m_cacheShaderMap[shader_action.info.shaderName]->compileShader();
			}
			else
			{
				LOG_FATAL("Fatal error on shader action. there is an undefine behavior and it should never happen!");
			}
		}
	}
}

void engine::shaderCompiler::ShaderCompiler::buildCallbackShader()
{
	{
		m_cacheShaderMap[fallbackShaderName::gbuffer] = std::make_shared<ShaderInfo>();
		auto& shaderInfo = *m_cacheShaderMap[fallbackShaderName::gbuffer];
		shaderInfo.passType = EShaderPass::GBuffer;
		shaderInfo.compiled = true;
		shaderInfo.compiled_suceess = true;
		shaderInfo.compiling = false;
		shaderInfo.compiled_vertShaderPath = fallbackShaderPath::gbufferVert;
		shaderInfo.compiled_fragShaderPath = fallbackShaderPath::gbufferFrag;
		shaderInfo.shaderName = fallbackShaderName::gbuffer;
		shaderInfo.effect = std::make_shared<ShaderEffect>();
		shaderInfo.effect->addStage(VulkanRHI::get()->getShader(shaderInfo.compiled_vertShaderPath),VK_SHADER_STAGE_VERTEX_BIT);
		shaderInfo.effect->addStage(VulkanRHI::get()->getShader(shaderInfo.compiled_fragShaderPath),VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderInfo.effect->reflectLayout(VulkanRHI::get()->getDevice(),nullptr,0);
	}

	{
		m_cacheShaderMap[fallbackShaderName::depth] = std::make_shared<ShaderInfo>();
		auto& shaderInfo = *m_cacheShaderMap[fallbackShaderName::depth];
		shaderInfo.passType = EShaderPass::ShadowDepth;
		shaderInfo.compiled = true;
		shaderInfo.compiled_suceess = true;
		shaderInfo.compiling = false;
		shaderInfo.compiled_vertShaderPath = fallbackShaderPath::depthVert;
		shaderInfo.compiled_fragShaderPath = fallbackShaderPath::depthFrag;
		shaderInfo.shaderName = fallbackShaderName::depth;
		shaderInfo.effect = std::make_shared<ShaderEffect>();
		shaderInfo.effect->addStage(VulkanRHI::get()->getShader(shaderInfo.compiled_vertShaderPath),VK_SHADER_STAGE_VERTEX_BIT);
		shaderInfo.effect->addStage(VulkanRHI::get()->getShader(shaderInfo.compiled_fragShaderPath),VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderInfo.effect->reflectLayout(VulkanRHI::get()->getDevice(),nullptr,0);
	}
}

void engine::shaderCompiler::ShaderCompiler::shaderFileWatch(std::string path_to_watch, engine::FileWatcher::FileStatus status,std::filesystem::file_time_type timeType)
{
	// 我们仅对 .shader 结尾的文件修改做处理
	if(FileSystem::getFileSuffixName(path_to_watch) != ".shader") return;

	// 不符合shader格式的shader文件不予编译
	ShaderInfo info;
	if(!compileShader(path_to_watch,info))
	{
		LOG_WARN("Shader file {0} compiled error!",path_to_watch);
		return;
	}

	ShaderAction shader_action;
	shader_action.info = info;
	shader_action.info.lastEditTime = std::to_string(to_time_t(timeType));
	switch(status)
	{
	case engine::FileWatcher::FileStatus::Created:
		shader_action.action = ShaderAction::Action::Add;
		break;

	case engine::FileWatcher::FileStatus::Modified:
		shader_action.action = ShaderAction::Action::Modified;
		break;

	case engine::FileWatcher::FileStatus::Erased:
		shader_action.action = ShaderAction::Action::Remove;
		break;

	default:
		LOG_FATAL("Fatal Error on file watcher!");
		break;
	}

	// 如果重名则直接覆盖掉
	m_dirtyShaderMaps[path_to_watch] = shader_action;
}

ShaderCompact engine::shaderCompiler::ShaderCompiler::getFallbackShader(EShaderPass passType)
{
	ShaderCompact ret{};

	if(passType==EShaderPass::Tonemapper)
	{
		ret.vertex = VulkanRHI::get()->getShader(fallbackShaderPath::tonemapperVert);
		ret.frag   = VulkanRHI::get()->getShader(fallbackShaderPath::tonemapperFrag);
	}
	else if(passType==EShaderPass::GBuffer)
	{
		ret.vertex = VulkanRHI::get()->getShader(fallbackShaderPath::gbufferVert);
		ret.frag   = VulkanRHI::get()->getShader(fallbackShaderPath::gbufferFrag);
	}
	else if(passType==EShaderPass::Lighting)
	{
		ret.vertex = VulkanRHI::get()->getShader(fallbackShaderPath::lightingVert);
		ret.frag   = VulkanRHI::get()->getShader(fallbackShaderPath::lightingFrag);
	}
	else if(passType==EShaderPass::ShadowDepth)
	{
		ret.vertex = VulkanRHI::get()->getShader(fallbackShaderPath::depthVert);
		ret.frag   = VulkanRHI::get()->getShader(fallbackShaderPath::depthFrag);
	}
	else
	{
		LOG_FATAL("No such shader pass Type! this is an developer bug!");
	}

	return ret;
}

engine::shaderCompiler::ShaderCompiler::ShaderCompiler(Ref<ModuleManager> manager) 
	: IRuntimeModule(manager),
	m_engineShadersWatcher(engine::FileWatcher(s_engineShader,std::chrono::milliseconds(500),
		"engineShaderWatcher",
		[&](std::string path_to_watch, engine::FileWatcher::FileStatus status,std::filesystem::file_time_type timeType) -> void {
	shaderFileWatch(path_to_watch,status,timeType);
}))
{
	const auto& cachePathMap = m_engineShadersWatcher.getCachePaths();

	// 初始化时将所有位于文件夹下的shader都存到脏shader map中
	for(auto& pair:cachePathMap)
	{
		if(FileSystem::getFileSuffixName(pair.first) == ".shader")
		{
			ShaderAction action;
			action.action = ShaderAction::Action::Add;
			action.info.lastEditTime = std::to_string(to_time_t(pair.second));
			if(compileShader(pair.first,action.info))
			{
				m_dirtyShaderMaps[pair.first] = action;
			}
		}
	}
}

ShaderCompact engine::shaderCompiler::ShaderCompiler::getShader(const std::string& shaderName,EShaderPass passType)
{
	if(containShader(shaderName))
	{
		ShaderCompact ret{};
		auto& shaderpassInfo = *m_cacheShaderMap[shaderName];

		// pass type符合并且编译完成
		if(shaderpassInfo.passType==passType && shaderpassInfo.compiled_suceess && shaderpassInfo.compiled && !shaderpassInfo.compiling)
		{
			ret.vertex = VulkanRHI::get()->getShader(shaderpassInfo.compiled_vertShaderPath);
			ret.frag   = VulkanRHI::get()->getShader(shaderpassInfo.compiled_fragShaderPath);

			ASSERT(ret.vertex && ret.frag, "Don't edit media folder because it is very important which store engine neccesory assets.");
			return ret;
		}
	}

	// fallback
	return getFallbackShader(passType);
}

std::shared_ptr<ShaderInfo> engine::shaderCompiler::ShaderCompiler::getMeshPassShaderInfo(const std::string& shaderName,EShaderPass passType)
{
	if(containShader(shaderName) && m_cacheShaderMap[shaderName]->compiled_suceess && m_cacheShaderMap[shaderName]->compiled)
	{
		return m_cacheShaderMap[shaderName];
	}
	else if(passType == EShaderPass::GBuffer)
	{
		return m_cacheShaderMap[fallbackShaderName::gbuffer];
	}
	else if(passType==EShaderPass::ShadowDepth)
	{
		return m_cacheShaderMap[fallbackShaderName::depth];
	}

	return nullptr;
}

bool compileShaderInner(ShaderInfo* info,const std::string& shaderFileName,const std::string& shaderin,EShaderStage stage)
{
	auto setName = [&](const std::string& finalName)
	{
		if(stage==EShaderStage::Vert)
		{
			info->compiled_vertShaderPath = finalName;
		}
		else if(stage==EShaderStage::Frag)
		{
			info->compiled_fragShaderPath = finalName;
		}
	};

	std::string originalShaderPath = FileSystem::getFolderName(shaderin);

	// 放到engine cache中的spv
	std::string finalName = s_engineShaderCache + shaderFileName + ".spv";

	// 中间文本文件
	std::string tmpShaderName = originalShaderPath + shaderFileName;

	std::ifstream fin(finalName);
	if(!fin)
	{
		LOG_TRACE("Miss shader cache {0}, compiling...",finalName);
	}
	else // 初始化编译时，若存在上一次编译后留下的文件则跳过编译直接使用原来的结果
	{
		LOG_TRACE("Found shader cache {0}.",finalName);
		setName(finalName);
		return true;
	}

	std::stringstream ss;

	ss << "// Shader Pass: " << toString(info->passType) << std::endl;

	std::ifstream sin(shaderin);
	if(!sin)
	{
		LOG_WARN("shader {0} can't found!",shaderin);
		return false;
	}

	std::ofstream os(tmpShaderName);
	if(!os)
	{
		LOG_WARN("Open file {0} error and compiling error.",tmpShaderName);
		return false;
	}

	std::string s = "";
	bool bHasPragmaVersion = false;
	while ( getline(sin,s))
	{
		if(!bHasPragmaVersion)
		{
			os << s << std::endl;
			FileSystem::removeEmptySpace(s);
			if(FileSystem::startWith(s,"#version"))
			{
				bHasPragmaVersion = true;
			}
			os << std::endl << ss.str() << std::endl;
		}
		else
		{
			os << s << std::endl;
		}
	}

	os.close();
	sin.close();

	// 调用glslang.exe编译为spv
	std::stringstream command;
	command << s_shaderCompile;
	command << " " << tmpShaderName << " -o " << finalName;
	system(command.str().c_str());

	if(cVarRemoveShaderString.get()!=0)
	{
		remove(tmpShaderName.c_str());
	}

	VulkanRHI::get()->getShader(finalName);

	setName(finalName);
	return true;
}


void engine::shaderCompiler::ShaderInfo::compileShader()
{
	this->compiling = true;
	this->compiled = false;

	jobsystem::execute([&](){

		std::string cacheFileName = getShaderCacheName(*this);
		bool bCompileResult = true;
		std::string compileName = this->shaderName + toString(passType);

		auto compileSuccess = [&]()
		{
			this->compiling = false;
			this->compiled = true;
			this->compiled_suceess = true;
			LOG_TRACE("Shader {0} has compiled!",compileName);
		};

		auto compileFailed = [&]()
		{
			this->compiling = false;
			this->compiled = true;
			this->compiled_suceess = false;
			LOG_TRACE("Shader {0} failed to compile!",compileName);
		};

		std::string cacheFileVertexShaderName = cacheFileName + ".vert";
		std::string cacheFileFragShaderName   = cacheFileName + ".frag";

		bCompileResult |= compileShaderInner(this,cacheFileVertexShaderName,this->vertShaderPath,EShaderStage::Vert);
		bCompileResult |= compileShaderInner(this,cacheFileFragShaderName,this->fragShaderPath,EShaderStage::Frag);

		// compile failed.
		if( !bCompileResult)
		{
			compileFailed();
			return;
		}

		// 构建shader effect
		this->effect = std::make_shared<ShaderEffect>();
		this->effect->addStage(VulkanRHI::get()->getShader(this->compiled_vertShaderPath),VK_SHADER_STAGE_VERTEX_BIT);
		this->effect->addStage(VulkanRHI::get()->getShader(this->compiled_fragShaderPath),VK_SHADER_STAGE_FRAGMENT_BIT);
		this->effect->reflectLayout(VulkanRHI::get()->getDevice(),nullptr,0);
		
		if(this->passType==EShaderPass::GBuffer   ||
		   this->passType==EShaderPass::ShadowDepth)
		{
			g_meshPassShouldRebuild = true;
		}
		else
		{
			g_globalPassShouldRebuild = true; 
		}
		compileSuccess();
	});
}

engine::shaderCompiler::ShaderInfo::ShaderInfo()
{
	passType = EShaderPass::Unknow;
	vertShaderPath = "";
	fragShaderPath = "";
	compiled_fragShaderPath = "";
	compiled_vertShaderPath = "";
	compiled = false;
	compiling = false;
	compiled_suceess = false;
	effect = nullptr;
	shaderName = "";
	lastEditTime = "";
}


