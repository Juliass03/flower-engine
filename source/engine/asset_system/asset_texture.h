#pragma once
#include "asset_common.h"
#include <vulkan/vulkan.h>

namespace engine{ namespace asset_system{

enum class ESamplerType
{
    PointClamp = 0,
    PointRepeat,
    LinearClamp,
    LinearRepeat,
};

struct TextureInfo
{
    uint64_t textureSize;
    EAssetFormat format;
    bool srgb;
    ESamplerType samplerType;
    ECompressMode compressMode;
    uint32 pixelSize[3];
    std::string originalFile;

    uint32 mipmapLevels;
    bool bCacheMipmaps;

    VkFormat getVkFormat();
};

extern TextureInfo readTextureInfo(AssetFile* file);
extern void unpackTexture(TextureInfo* info,const char* srcBuffer,size_t srcSize,char* dest);
extern AssetFile packTexture(TextureInfo* info,void* pixelData);

extern bool bakeTexture(const char* pathIn,const char* pathOut,bool srgb,bool compress,uint32 req_comp,bool bGenerateMipmap);

}}