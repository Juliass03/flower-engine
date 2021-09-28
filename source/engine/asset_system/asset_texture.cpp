#include "asset_texture.h"
#include "asset_system.h"
#include <nlohmann/json.hpp>
#include <lz4/lz4.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace engine{ namespace asset_system{

uint32 toUint32(ESamplerType type)
{
    return static_cast<uint32>(type);
}

ESamplerType toSamplerType(uint32 in)
{
    return static_cast<ESamplerType>(in);
}

std::string toString(EAssetFormat format)
{
	switch(format)// NOTE: Vulkan不支持R8G8B8格式
	{
	case EAssetFormat::T_R8G8B8A8:
		return "T_R8G8B8A8";
    default:
        LOG_IO_FATAL("Unkown texture format!");
        return "Unknown";
	}
}

AssetFile packTexture(TextureInfo* info,void* pixelData)
{
    AssetFile file;

    // 填入Magic头
    file.type[0] = 'T';
    file.type[1] = 'E';
    file.type[2] = 'X';
    file.type[3] = 'I';
    file.version = 1;

    // meta data 填入
    nlohmann::json textureMetadata;
    textureMetadata["format"] = toString(info->format);
    textureMetadata["width"] = info->pixelSize[0];
    textureMetadata["height"] = info->pixelSize[1];
    textureMetadata["depth"] = info->pixelSize[2];
    textureMetadata["bufferSize"] = info->textureSize;
    textureMetadata["originalFile"] = info->originalFile;
    textureMetadata["srgb"] = info->srgb;
    textureMetadata["sampler"] = toUint32(info->samplerType);

    // LZ4 压缩
    if(info->compressMode == ECompressMode::LZ4)
    {
        auto compressStaging = LZ4_compressBound((int)info->textureSize);
        file.binBlob.resize(compressStaging);
        auto compressedSize = LZ4_compress_default((const char*)pixelData,file.binBlob.data(),(int)info->textureSize,compressStaging);
        file.binBlob.resize(compressedSize);
        textureMetadata["compression"] = "LZ4";
    }
    else
    {
        file.binBlob.resize(info->textureSize);
        memcpy(file.binBlob.data(),(const char*)pixelData,info->textureSize);
        textureMetadata["compression"] = "None";
    }

    std::string stringified = textureMetadata.dump();
    file.json = stringified;

    return file;
}

TextureInfo readTextureInfo(AssetFile* file)
{
    TextureInfo info;
    nlohmann::json textureMetadata = nlohmann::json::parse(file->json);

    std::string format_string = textureMetadata["format"];
    info.format = toFormat(format_string.c_str());

    std::string compression_string = textureMetadata["compression"];
    info.compressMode = toCompressMode(compression_string.c_str());

    info.textureSize  = textureMetadata["bufferSize"];
    info.originalFile = textureMetadata["originalFile"];

    info.pixelSize[0] = textureMetadata["width"];
    info.pixelSize[1] = textureMetadata["height"];
    info.pixelSize[2] = textureMetadata["depth"];

    info.srgb = textureMetadata["srgb"];
    info.samplerType = toSamplerType(textureMetadata["sampler"]);

    return info;
}

void unpackTexture(TextureInfo* info,const char* sourceBuffer,size_t sourceSize,char* destination)
{
    if(info->compressMode == ECompressMode::LZ4)
    {
        LZ4_decompress_safe(sourceBuffer,destination,(int)sourceSize,(int)info->textureSize);
    }
    else
    {
        memcpy(destination,sourceBuffer,sourceSize);
    }
}

EAssetFormat castToAssetFormat(int32 channels)
{
    CHECK(channels == 4);
    switch(channels)
    {
    case 4: return EAssetFormat::T_R8G8B8A8;
    }
    return EAssetFormat::Unknown;
}

bool bakeTexture(const char* pathIn,const char* pathOut,bool srgb,bool compress,uint32 req_comp)
{
    CHECK(req_comp == 4);
    int32 texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(pathIn, &texWidth, &texHeight, &texChannels, req_comp);

    if (!pixels) 
    {
        LOG_IO_FATAL("Fail to load image {0}.",pathIn);
    }

    int32 imageSize = texWidth * texHeight * req_comp;

    TextureInfo info;
    info.format = castToAssetFormat(req_comp);
    info.originalFile = pathIn;
    info.textureSize = imageSize;
    info.pixelSize[0] = texWidth;
    info.pixelSize[1] = texHeight;
    info.srgb = srgb;
    info.samplerType = ESamplerType::LinearRepeat;

    // stb 库支持的图片均为1的深度
    info.pixelSize[2] = 1;
    info.compressMode = compress ? ECompressMode::LZ4 : ECompressMode::None;

    AssetFile newImage = packTexture(&info,pixels);
    stbi_image_free(pixels);
    return saveBinFile(pathOut,newImage);
}

}}