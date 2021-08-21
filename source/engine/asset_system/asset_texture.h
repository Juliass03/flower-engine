#pragma once
#include "asset_common.h"

namespace engine{ namespace asset_system{

struct TextureInfo
{
    uint64_t textureSize;
    EAssetFormat format;
    ECompressMode compressMode;
    uint32 pixelSize[3];
    std::string originalFile;
};

extern TextureInfo readTextureInfo(AssetFile* file);
extern void unpackTexture(TextureInfo* info,const char* srcBuffer,size_t srcSize,char* dest);
extern AssetFile packTexture(TextureInfo* info,void* pixelData);

extern bool bakeTexture(const char* pathIn,const char* pathOut,bool compress,uint32 req_comp);

}}