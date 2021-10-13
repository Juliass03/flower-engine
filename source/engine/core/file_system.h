#pragma once
#include <filesystem>
#include <regex>
#include <string>
#include <sstream>
#include <algorithm>
#include "core.h"

namespace engine
{

static const char* s_logFilePath         = "./log/";
static const char* s_fontsDir            = "./media/font/";
static const char* s_projectDir          = "./project/"; 
static const char* s_projectDirRaw       = "./project";
static const char* s_mediaDir            = "./media/";
static const char* s_iconPath            = "./media/icon/flower.png";
static const char* s_projectSuffix       = ".flower";
static const char* s_engineTex           = "./media/engine_texture/";
static const char* s_engineMesh          = "./media/engine_mesh/";
static const char* s_engineShader        = "./media/shader/";
static const char* s_engineShaderCache   = "./media/cache/shader/";
static const char* s_shaderCompile       = "glslc.exe";

static const char* s_defaultWhiteTextureName      = "./media/engine_texture/T_White.tga";
static const char* s_defaultBlackTextureName      = "./media/engine_texture/T_Black.tga";
static const char* s_defaultNormalTextureName     = "./media/engine_texture/T_DefaultNormal.tga";
static const char* s_defaultCheckboardTextureName = "./media/engine_texture/T_Checkerboard_SRGB.tga";
static const char* s_defaultEmissiveTextureName   = "./media/engine_texture/T_DefaultEmissive_SRGB.tga";

static const char* s_engineMeshBox  = "./media/engine_mesh/box.mesh";


// engine shader path
static const char* s_shader_tonemapper = "Engine/Tonemapper";
static const char* s_shader_lighting   = "Engine/Lighting";
static const char* s_shader_depth      = "Engine/Depth";

class FileSystem
{
public:
    // 移除空字符
    inline static void removeEmptySpace( std::string& inOut)
    {
        std::string::iterator end_pos = std::remove(inOut.begin(),inOut.end(),' ');
        inOut.erase(end_pos,inOut.end());
    }

    // 是否以字符串substr开头
    inline static bool startWith(const std::string& in, const std::string& substr)
    {
        CHECK(in.size() != 0);
        return in.find(substr) == 0;
    }

    // 是否以字符串substr结尾
    inline static bool endWith(const std::string& in,const std::string& substr)
    {
        return in.rfind(substr) == (in.length() - substr.length());
    }

    // 去除头部字符
    inline static std::string removeFirstChar(const std::string& in)
    {
        CHECK(in.size()>= 2);
        return in.substr(1,in.size()-1);
    }

    // 去除头尾两个字符
    inline static std::string removeFirstAndLastChar(const std::string& in)
    {
        CHECK(in.size() >= 2);
        return in.substr(1, in.size() - 2);
    }

    // 检测是否全为 A - Z组成的字符串
    inline static bool allCharBetween_A_Z(const std::string& in)
    {
        CHECK(in.size() != 0);
        for (auto& charVal : in)
        {
            if(charVal > 'Z'||charVal <'A')
            {
                return false;
            }
        }

        return true;
    }

    // 检测是否全为 a - z组成的字符串
    inline bool static allCharBetween_a_z(const std::string& in)
    {
        CHECK(in.size()!=0);
        for(auto& charVal:in)
        {
            if(charVal>'z'||charVal<'a')
            {
                return false;
            }
        }

        return true;
    }

    // 检测是否全为 0 - 9组成的字符串
    inline bool static allCharBetween_0_9(const std::string& in)
    {
        CHECK(in.size()!=0);
        for(auto& charVal:in)
        {
            if(charVal>'9'||charVal<'0')
            {
                return false;
            }
        }

        return true;
    }

    // 检测是否全为英文字母和数字组成的字符串
    inline bool static allCharBetweenEnCharAndNum(const std::string& in)
    {
        CHECK(in.size()!=0);
        for(auto& charVal:in)
        {
            if(
                !((charVal >= '0' && charVal <= '9') ||
                    (charVal  >= 'a' && charVal <= 'z') ||
                    (charVal  >= 'A' && charVal <= 'Z'))
                ){
                return false;
            }
        }

        return true;
    }

    // 检测是否全为英文字母和数字以及下划线组成的字符串
    inline bool static allCharBetweenEnCharAndNumAnd_(const std::string& in)
    {
        CHECK(in.size()!=0);
        for(auto& charVal:in)
        {
            if(
                !((charVal>='0'&&charVal<='9') ||
                    (charVal>='a'&&charVal<='z') ||
                    (charVal>='A'&&charVal<='Z') ||
                    (charVal == '_'))
                )
            {
                return false;
            }
        }

        return true;
    }

    // 检测是否全为英文字母和数字以及下划线组成的字符串，并且以英文字母开头
    inline bool static allCharBetweenEnCharAndNumAnd_AndEnStart(const std::string& in)
    {
        CHECK(in.size()!=0);
        int32 index = 0;
        for(auto& charVal:in)
        {
            if(index==0)
            {
                if( !((charVal >= 'a' && charVal <= 'z')||
                    (charVal >= 'A' && charVal <= 'Z'))
                    )
                {
                    return false;
                }

                // 只需判断一次
                index ++;
            }

            if(
                !((charVal>='0'&&charVal<='9')||
                    (charVal>='a'&&charVal<='z')||
                    (charVal>='A'&&charVal<='Z')||
                    (charVal=='_'))
                )
            {
                return false;
            }
        }

        return true;
    }

    static inline std::string toCommonPath(const std::filesystem::directory_entry& entry)
    {
        std::stringstream path;
        path << entry.path();
        std::string pathStr = path.str();
        // \\ 符号换成 /
        pathStr = std::regex_replace(pathStr,std::regex("\\\\\\\\"),"/");

        // 把路径符号中的" 去掉
        pathStr = std::regex_replace(pathStr,std::regex("\""),"");
        return pathStr;
    }

    static inline std::string toCommonPath(const std::filesystem::path& entry)
    {
        std::stringstream path;
        path << entry;
        std::string pathStr = path.str();
        // \\ 符号换成 /
        pathStr = std::regex_replace(pathStr,std::regex("\\\\\\\\"),"/");

        // 把路径符号中的" 去掉
        pathStr = std::regex_replace(pathStr,std::regex("\""),"");
        return pathStr;
    }

    // eg: ./Content/Mesh/A.obj -> ./Content/Mesh/
    inline static std::string getFolderName(const std::string& in)
    {
        std::string rawName = in.substr(0, in.find_last_of("/\\") + 1);
        return rawName;
    }

    // eg: ./Content/Mesh/A.obj -> A.obj
    inline static std::string getFileName(const std::string& in)
    {
        size_t pos = in.find_last_of("/\\") + 1;
        std::string fileName;
        if(pos != std::string::npos)
        {
            fileName = in.substr(pos);
        }
        return fileName;
    }

    // eg: ./Content/Mesh/A.obj -> .obj
    inline static std::string getFileSuffixName(const std::string& in)
    {
        return in.substr(in.find_last_of('.'));
    }

    // eg: ./Content/Mesh/A.obj -> ./Content/Mesh/A
    inline static std::string getFileRawName(const std::string& pathStr)
    {
        return pathStr.substr(0, pathStr.rfind("."));
    }

    // eg: ./Content/Mesh/A -> A
    inline static std::string getFolderRawName(const std::string& pathStr)
    {
        return pathStr.substr(pathStr.find_last_of("/\\") +1);
    }

    // eg: ./Content/Mesh/A.obj -> obj
    inline static std::string getFileSuffixNameWithOutDot(const std::string& in)
    {
        return in.substr(in.find_last_of('.') + 1);
    }

    // eg: ./Content/Mesh/A.obj -> A
    inline static std::string getFileNameWithoutSuffix(const std::string& in)
    {
        std::string rawName = in.substr(0, in.rfind("."));
        size_t pos = rawName.find_last_of("/\\") + 1;
        std::string fileName;
        if(pos != std::string::npos)
        {
            fileName = rawName.substr(pos);
        }
        return fileName;
    }

    // eg: ./Content/Mesh/A.obj -> ./Content/Mesh/A.A
    inline static std::string filePathNameToGameName(const std::string& in)
    {
        std::string rawName = getFileRawName(in);
        rawName += ".";
        rawName += getFileNameWithoutSuffix(in);
        return rawName;
    }
};

#include <chrono>
#include <thread>
#include <unordered_map>
#include <functional>

class FileWatcher
{
public:
    enum class FileStatus
    {
        Created,
        Modified,
        Erased,
    };

    using action = void(std::string,FileStatus,std::filesystem::file_time_type);

private:
    std::string m_name;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_paths;
    std::string m_watchPath;
    std::chrono::duration<int, std::milli> m_delay;
    bool m_runing;

    std::thread m_t;

    bool contains(const std::string& key)
    {
        auto el = m_paths.find(key);
        return el != m_paths.end();
    }

    std::function<action> m_action;
    
public:
    ~FileWatcher()
    {
        m_runing = false;
        if(m_t.joinable()) m_t.join();
    }

    void end()
    {
        m_runing = false;
        if(m_t.joinable()) m_t.join();
    }

    const std::unordered_map<std::string,std::filesystem::file_time_type>& getCachePaths() const
    {
        return m_paths;
    }

    FileWatcher(const std::string& path,std::chrono::duration<int,std::milli> delay,const std::string& name,const std::function<void(std::string,FileStatus,std::filesystem::file_time_type)>& action)
        : m_watchPath(path),m_delay(delay),m_name(name)
    {
        for(auto& file:std::filesystem::recursive_directory_iterator(m_watchPath))
        {
            m_paths[file.path().string()] = std::filesystem::last_write_time(file);
        }

        m_runing = true;
        m_action = action;

        m_t = std::thread([&](){

            while(m_runing)
            {
                std::this_thread::sleep_for(m_delay);
                auto it = m_paths.begin();

                while(it != m_paths.end())
                {
                    if(!std::filesystem::exists(it->first))
                    {
                        m_action(it->first, FileStatus::Erased,it->second);
                        it = m_paths.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }

                for(auto& file:std::filesystem::recursive_directory_iterator(m_watchPath))
                {
                    auto current_file_last_write_time = std::filesystem::last_write_time(file);

                    // 创建文件
                    if(!contains(file.path().string()))
                    {
                        m_paths[file.path().string()] = current_file_last_write_time;
                        m_action(file.path().string(),FileStatus::Created,current_file_last_write_time);
                    }
                    else
                    {
                        // 文件修改
                        if(m_paths[file.path().string()]!=current_file_last_write_time)
                        {
                            m_paths[file.path().string()] = current_file_last_write_time;
                            m_action(file.path().string(),FileStatus::Modified,current_file_last_write_time);
                        }
                    }
                }
            }
            LOG_INFO("Thread {0} ending.",m_name);
        });
    }
};

}

