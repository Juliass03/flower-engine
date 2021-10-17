#include "asset_texture.h"
#include "asset_system.h"
#include <nlohmann/json.hpp>
#include <lz4/lz4.h>
#include "../vk/impl/vk_common.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace engine{ namespace asset_system{

// NOTE: 获得输入纹理的所有Mipmap像素数
uint32 getTextureMipmapPixelCount(uint32_t width,uint32_t height,uint32_t componentCount,uint32_t mipMapCount)
{
    if(mipMapCount == 0) return 0;
    if(componentCount == 0) return 0;

    auto mipPixelCount = width * height * componentCount; // mip0
    if(mipMapCount == 1) return mipPixelCount;

	uint32 mipWidth = width;
	uint32 mipHeight = height;

	for(uint32_t mipmapLevel = 1; mipmapLevel < mipMapCount; mipmapLevel++)
	{
		const uint32 currentMipWidth  = mipWidth  > 1 ? mipWidth  / 2 : 1;
		const uint32 currentMipHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        mipPixelCount += currentMipWidth * currentMipHeight * componentCount;

		if(mipWidth > 1) mipWidth   /= 2;
		if(mipHeight> 1) mipHeight /= 2;
	}
    return mipPixelCount;
}

inline unsigned char srgbToLinear(unsigned char inSrgb)
{
    float srgb = inSrgb / 255.0f;
    srgb = glm::max(6.10352e-5f, srgb);
    float lin = srgb > 0.04045f ? glm::pow( srgb * (1.0f / 1.055f) + 0.0521327f, 2.4f ) : srgb * (1.0f / 12.92f);

    return unsigned char(lin * 255.0f);
}

inline unsigned char linearToSrgb(unsigned char inlin)
{
    float lin = inlin / 255.0f;
	if(lin < 0.00313067f) return unsigned char(lin * 12.92f * 255.0f);
	float srgb = glm::pow(lin,(1.0f/2.4f)) * 1.055f - 0.055f;

    return unsigned char(srgb * 255.0f);
}

static bool mipmapGenerate(const unsigned char* data,std::vector<unsigned char>& result, uint32_t width,uint32_t height,uint32_t componentCount,bool bSrgb)
{
    if(componentCount != 4) return false;
    if(width!=height)
    {
        // 非2的次方纹理的mipmap暂时不打算生成
        // 之后有空引入3x3的滤波器在做处理
        return false;
    }

    uint32_t mipmapCount = getMipLevelsCount(width,height);

    if(mipmapCount <= 1)
    {
        return false;
    }

    uint32_t mipPixelSize = getTextureMipmapPixelCount(width,height,componentCount,mipmapCount);
    result.clear();
    result.resize(0);
    result.resize(mipPixelSize);
        
	uint32 mipWidth = width;
	uint32 mipHeight = height;

    // NOTE: 将Mip0数据拷贝到容器中
    uint32 mipPtrPos = width * height * componentCount;
    uint32 lastMipPtrPos = 0;
    memcpy((void*)result.data(),(void*)data,size_t(mipPtrPos));

    // NOTE: 我们从第一级Mipmap开始逐级生成
    for(uint32_t mipmapLevel = 1; mipmapLevel < mipmapCount; mipmapLevel++) 
    {
        CHECK(mipPtrPos < mipPixelSize);

        CHECK(mipWidth  > 1);
        CHECK(mipHeight > 1);
        const uint32 lastMipWidth  = mipWidth;
        const uint32 lastMipHeight = mipHeight;
        const uint32 currentMipWidth  = mipWidth  / 2; 
        const uint32 currentMipHeight = mipHeight / 2; 
 
        // 处理当前Mip层级的所有的像素
        for(uint32 pixelPosX = 0; pixelPosX < currentMipWidth; pixelPosX ++)
        {
            for(uint32 pixelPosY = 0; pixelPosY < currentMipHeight; pixelPosY ++)
            {
                // NOTE: 处理中的像素
                unsigned char* currentMipPixelOffset = result.data() + mipPtrPos + (pixelPosX + pixelPosY * currentMipWidth) * componentCount;
                unsigned char* redComp   = currentMipPixelOffset;
                unsigned char* greenComp = currentMipPixelOffset + 1;
                unsigned char* blueComp  = currentMipPixelOffset + 2;
                unsigned char* alphaComp = currentMipPixelOffset + 3;

                // (0,0)像素
                const uint32 lastMipPixelPosX00 = pixelPosX * 2;
                const uint32 lastMipPixelPosY00 = pixelPosY * 2;

                // (1,0)像素
				const uint32 lastMipPixelPosX10 = pixelPosX * 2 + 1;
				const uint32 lastMipPixelPosY10 = pixelPosY * 2 + 0;

                // (0,1)像素
				const uint32 lastMipPixelPosX01 = pixelPosX * 2 + 0;
				const uint32 lastMipPixelPosY01 = pixelPosY * 2 + 1;

                // (1,1)像素
				const uint32 lastMipPixelPosX11 = pixelPosX * 2 + 1;
				const uint32 lastMipPixelPosY11 = pixelPosY * 2 + 1;

                bool bSRGBProcess = false;

                const unsigned char* lastMipPixelOffset00 = result.data() + lastMipPtrPos + (lastMipPixelPosX00 + lastMipPixelPosY00 * lastMipWidth) * componentCount;
                const unsigned char* lastMipPixelOffset10 = result.data() + lastMipPtrPos + (lastMipPixelPosX10 + lastMipPixelPosY10 * lastMipWidth) * componentCount;
                const unsigned char* lastMipPixelOffset01 = result.data() + lastMipPtrPos + (lastMipPixelPosX01 + lastMipPixelPosY01 * lastMipWidth) * componentCount;
                const unsigned char* lastMipPixelOffset11 = result.data() + lastMipPtrPos + (lastMipPixelPosX11 + lastMipPixelPosY11 * lastMipWidth) * componentCount;

                const unsigned char redLastMipPixel_00    = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset00))     : *(lastMipPixelOffset00);
                const unsigned char greenLastMipPixel_00  = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset00 + 1)) : *(lastMipPixelOffset00 + 1);
                const unsigned char blueLastMipPixel_00   = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset00 + 2)) : *(lastMipPixelOffset00 + 2);
                const unsigned char* alphaLastMipPixel_00 = lastMipPixelOffset00 + 3;

				const unsigned char redLastMipPixel_10    = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset10))     : *(lastMipPixelOffset10);
				const unsigned char greenLastMipPixel_10  = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset10 + 1)) : *(lastMipPixelOffset10 + 1);
				const unsigned char blueLastMipPixel_10   = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset10 + 2)) : *(lastMipPixelOffset10 + 2);
				const unsigned char* alphaLastMipPixel_10 = lastMipPixelOffset10 + 3;

				const unsigned char redLastMipPixel_01    = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset01))     : *(lastMipPixelOffset01);
				const unsigned char greenLastMipPixel_01  = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset01 + 1)) : *(lastMipPixelOffset01 + 1);
				const unsigned char blueLastMipPixel_01   = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset01 + 2)) : *(lastMipPixelOffset01 + 2);
				const unsigned char* alphaLastMipPixel_01 = lastMipPixelOffset01 + 3;

				const unsigned char redLastMipPixel_11    = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset11))     : *(lastMipPixelOffset11);
				const unsigned char greenLastMipPixel_11  = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset11 + 1)) : *(lastMipPixelOffset11 + 1);
				const unsigned char blueLastMipPixel_11   = bSRGBProcess ? srgbToLinear(*(lastMipPixelOffset11 + 2)) : *(lastMipPixelOffset11 + 2);
				const unsigned char* alphaLastMipPixel_11 = lastMipPixelOffset11 + 3;

                *alphaComp = (*alphaLastMipPixel_00 + *alphaLastMipPixel_10 + *alphaLastMipPixel_01 + *alphaLastMipPixel_11) / 4;
                *greenComp =  (greenLastMipPixel_00 + greenLastMipPixel_10 + greenLastMipPixel_01 + greenLastMipPixel_11) / 4;
                *blueComp  = (blueLastMipPixel_00 + blueLastMipPixel_10  + blueLastMipPixel_01  + blueLastMipPixel_11) / 4;
                *redComp   =  (redLastMipPixel_00  + redLastMipPixel_10 + redLastMipPixel_01 + redLastMipPixel_11) / 4;

                if(bSRGBProcess)
                {
                    *redComp = linearToSrgb(*redComp);
                    *greenComp = linearToSrgb(*greenComp);
                    *blueComp = linearToSrgb(*blueComp);
                }
            }
        }

        lastMipPtrPos = mipPtrPos;
        mipPtrPos += currentMipWidth * currentMipHeight * componentCount;
        
        mipWidth  /= 2;
        mipHeight /= 2;
    }

    return true;
}

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
    textureMetadata["mipmapLevels"] = info->mipmapLevels;
    textureMetadata["bCacheMipmaps"] = info->bCacheMipmaps;

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

    info.bCacheMipmaps = textureMetadata["bCacheMipmaps"];
    info.mipmapLevels = textureMetadata["mipmapLevels"];

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

bool bakeTexture(const char* pathIn,const char* pathOut,bool srgb,bool compress,uint32 req_comp,bool bGenerateMipmap)
{
    CHECK(req_comp == 4);
    int32 texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(pathIn, &texWidth, &texHeight, &texChannels, req_comp);

    if (!pixels) 
    {
        LOG_IO_FATAL("Fail to load image {0}.",pathIn);
    }

	std::vector<unsigned char> generateMips{};
    bool bGenerateMipsSucess = false;
	if(bGenerateMipmap)
	{
        bGenerateMipsSucess = mipmapGenerate((unsigned char*)pixels,generateMips,texWidth,texHeight,req_comp,srgb);
	}

    TextureInfo info;
	info.bCacheMipmaps = bGenerateMipsSucess;
	info.mipmapLevels = getMipLevelsCount(texWidth,texHeight);

    int32 imageSize = bGenerateMipsSucess ? getTextureMipmapPixelCount(texWidth,texHeight,req_comp,info.mipmapLevels) : texWidth * texHeight * req_comp;

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

    AssetFile newImage = bGenerateMipsSucess ? packTexture(&info,generateMips.data()) : packTexture(&info,pixels);

    stbi_image_free(pixels);
    return saveBinFile(pathOut,newImage);
}

}

VkFormat asset_system::TextureInfo::getVkFormat()
{
    switch(format)
    {
    case EAssetFormat::T_R8G8B8A8:
    {
        if(this->srgb)
        {
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        else
        {
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }
    default:
        LOG_FATAL("Unkonw format!");
    }
}

}