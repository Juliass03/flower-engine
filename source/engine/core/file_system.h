#pragma once
#include <filesystem>
#include <regex>
#include <string>
#include <sstream>
#include <algorithm>

namespace engine
{

static const char* s_logFilePath = "./log/";
static const char* s_fontsDir    = "./media/font/";
static const char* s_projectDir  = "./project/"; 
static const char* s_mediaDir    = "./media/";

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

}