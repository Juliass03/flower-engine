#pragma once
#include "../core/core.h"

namespace engine{ namespace asset_system{

struct AssetFile
{
    char type[4];
    uint32_t version;
    std::string json;
    std::vector<char> binBlob;
};

enum class ECompressMode
{
    None,
    LZ4,
};

extern bool saveBinFile(const char* path,const AssetFile& file);
extern bool loadBinFile(const char* path,AssetFile& out_file);
extern ECompressMode toCompressMode(const char* type);

enum class EAssetFormat
{
    Unknown = 0,

    T_R8G8B8A8, 
    T_R8G8, // NOTE: Vulkan不支持R8G8B8格式
    T_R8,
};

extern EAssetFormat toFormat(const char* f);

}}