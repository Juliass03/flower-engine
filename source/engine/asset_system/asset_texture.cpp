#include "asset_system.h"
#include "../core/file_system.h"
#include <filesystem>
#include <fstream>
#include "../vk/vk_rhi.h"
#include <stb/stb_image.h>
#include "../../imgui/imgui_impl_vulkan.h"
#include "../renderer/texture.h"
#include "asset_texture.h"
#include "asset_mesh.h"
#include "../launch/launch_engine_loop.h"
#include "../core/job_system.h"

#include <nlohmann/json.hpp>
#include <lz4/lz4.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_DXT_IMPLEMENTATION
#include <stb/stb_dxt.h>

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

// NOTE: 获得所有Mipmap层级的总像素数
uint32 getTextureMipmapPixelCountGpuCompress(uint32 texWidth,uint32 texHeight,uint32 mipmapCounts,EBlockType fmt)
{
    CHECK(texWidth >= 4);
    CHECK(texHeight >= 4);

    if(fmt==EBlockType::BC3)
    {
        // BC3 1/4 压缩率
        const auto totalMipCount = getMipLevelsCount(texWidth,texHeight);
        if(totalMipCount==mipmapCounts) // 有最后两级
        {
            const uint32 totalMipmapPixelCountWithoutFinal2Level = getTextureMipmapPixelCount(texWidth,texHeight,4,mipmapCounts);

            // 最后两级均视为4x4的块来压缩
            return (totalMipmapPixelCountWithoutFinal2Level - 1*1*4 - 2*2*4 + 4*4*4 * 2 ) /4;
        }
        else if(mipmapCounts==totalMipCount-1) // 没有最后一级
        {
            const uint32 totalMipmapPixelCountWithoutFinal2Level = getTextureMipmapPixelCount(texWidth,texHeight,4,mipmapCounts);
			return (totalMipmapPixelCountWithoutFinal2Level - 1*1*4 - 2*2*4 + 4*4*4)/4;
        }
        else if(mipmapCounts==totalMipCount-2) // 没有最后两级
        {
            return getTextureMipmapPixelCount(texWidth,texHeight,4,mipmapCounts) / 4;
        }
    }

    LOG_FATAL("ERROR Fmt");
    return 0;
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
    if(width!=height){ return false; }
    if(width < 4) return false;
    if(height < 4) return false;
    if(width & (width - 1)) return false; // 非2的幂次
    if(height & (height -1)) return false; // 非2的幂次


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

static bool mipmapCompressBC3(std::vector<unsigned char>& inData,std::vector<unsigned char>& result,uint32_t width,uint32_t height)
{
    static const uint32 componentCount = 4;
    static const uint32 blockSize = 4;

    uint32_t mipmapCount = getMipLevelsCount(width,height);
    CHECK(mipmapCount >= 3); // 最小两级Mipmap将使用倒数第三级Mipmap的数据，因为它们大小不足4x4

    result.resize(getTextureMipmapPixelCountGpuCompress(width,height,mipmapCount,EBlockType::BC3));

	uint32 mipWidth = width;
	uint32 mipHeight = height;

	uint32 mipPtrPos = 0;

    uint8 block[64] {};
    uint8* outBuffer = result.data();
	for(uint32_t mipmapLevel = 0; mipmapLevel < mipmapCount; mipmapLevel++)
	{
        CHECK(mipPtrPos < (uint32)inData.size());
		CHECK(mipWidth  >= 1);
		CHECK(mipHeight >= 1);

		// 处理当前Mip层级的所有的像素

        if(mipWidth>=4)
        {
            for(uint32 pixelPosY = 0; pixelPosY < mipHeight; pixelPosY += blockSize)
			{
                for(uint32 pixelPosX = 0; pixelPosX < mipWidth; pixelPosX += blockSize)
				{
					// 提出4x4像素块中的数据到block中
					uint32 blockLocation = 0;
                    for(uint32 j = 0; j < blockSize; j++)
					{
                        for(uint32 i = 0; i < blockSize; i++)
						{
							const uint32 dimX = pixelPosX + i;
							const uint32 dimY = pixelPosY + j;

							const uint32 pixelLocation = mipPtrPos+(dimX+dimY*mipWidth)*componentCount;
							unsigned char* dataStart = inData.data()+pixelLocation;

							// 按顺序填入四个通道的像素
							for(uint32 k = 0; k<componentCount; k++)
							{
								CHECK(blockLocation<64);
								block[blockLocation] = *dataStart; blockLocation++; dataStart++;
							}
						}
					}

					// 提取完毕后做压缩处理
					stb_compress_dxt_block(outBuffer,block,1,STB_DXT_HIGHQUAL);
					outBuffer += 16; // 偏移16个像素
				}
			}
        }
		else if(mipWidth==2 && mipHeight==2)
		{
             const uint32 pixelLocation00 = mipPtrPos + (0 + 0 * mipWidth) * componentCount;
             const uint32 pixelLocation01 = mipPtrPos + (0 + 1 * mipWidth) * componentCount;
             const uint32 pixelLocation10 = mipPtrPos + (1 + 0 * mipWidth) * componentCount;
             const uint32 pixelLocation11 = mipPtrPos + (1 + 1 * mipWidth) * componentCount;

             // 00 block填充
             for(uint32 j = 0; j<2; j++)
             {
                 for(uint32 i = 0; i<2; i++)
                 {
                    uint32 blockIndex = (i + j * 4) * 4;
                    block[blockIndex + 0] = *(inData.data()  + pixelLocation00 + 0);
                    block[blockIndex + 1] = *(inData.data()  + pixelLocation00 + 1);
                    block[blockIndex + 2] = *(inData.data()  + pixelLocation00 + 2);
                    block[blockIndex + 3] = *(inData.data()  + pixelLocation00 + 3);
                 }
             }

             // 01 block填充
             for(uint32 j = 0; j<2; j++)
             {
                 for(uint32 i = 0; i<2; i++)
				 {
					 uint32 blockIndex = (i+j*4)*4;
					 block[blockIndex+0] = *(inData.data()+pixelLocation01+0);
					 block[blockIndex+1] = *(inData.data()+pixelLocation01+1);
					 block[blockIndex+2] = *(inData.data()+pixelLocation01+2);
					 block[blockIndex+3] = *(inData.data()+pixelLocation01+3);
				 }
			 }

             // 10 block填充
             for(uint32 j = 0; j<2; j++)
             {
                 for(uint32 i = 0; i<2; i++)
				 {
					 uint32 blockIndex = (i+j*4)*4;
					 block[blockIndex+0] = *(inData.data()+pixelLocation10+0);
					 block[blockIndex+1] = *(inData.data()+pixelLocation10+1);
					 block[blockIndex+2] = *(inData.data()+pixelLocation10+2);
					 block[blockIndex+3] = *(inData.data()+pixelLocation10+3);
				 }
			 }

             // 11 block填充
             for(uint32 j = 0; j<2; j++)
             {
                 for(uint32 i = 0; i<2; i++)
				 {
					 uint32 blockIndex = (i+j*4)*4;
					 block[blockIndex+0] = *(inData.data()+pixelLocation11+0);
					 block[blockIndex+1] = *(inData.data()+pixelLocation11+1);
					 block[blockIndex+2] = *(inData.data()+pixelLocation11+2);
					 block[blockIndex+3] = *(inData.data()+pixelLocation11+3);
				 }
			 }

			 stb_compress_dxt_block(outBuffer,block,1,STB_DXT_HIGHQUAL);
			 outBuffer += 16; // 偏移16个像素
		}
		else if(mipWidth==1 && mipHeight==1)
		{
            uint8* dataStart = inData.data() + mipPtrPos;
            for(uint32 i = 0; i < 64; i += 4)
            {
                // 填入四个通道
                block[i + 0] = *(dataStart + 0);
                block[i + 1] = *(dataStart + 1);
                block[i + 2] = *(dataStart + 2);
                block[i + 3] = *(dataStart + 3);
            }
            stb_compress_dxt_block(outBuffer,block,1,STB_DXT_HIGHQUAL);
            // outBuffer += 16; // 偏移16个像素 // 最后一级无需偏移
		}

        // 将指针偏移到下一级Mip开始的像素位置
		mipPtrPos += mipWidth * mipHeight * componentCount;

		mipWidth  /= 2;
		mipHeight /= 2;
	}

    return true;
}

static bool mipmapCompressGpu(std::vector<unsigned char>& inData,std::vector<unsigned char>& result,uint32_t width,uint32_t height,uint32_t componentCount,bool bSrgb,EBlockType type)
{
    if(type == EBlockType::BC3)
    {
        return mipmapCompressBC3(inData,result,width,height);
    }

    return false;
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
    textureMetadata["bGpuCompress"] = info->bGpuCompress;
    textureMetadata["gpuCompressType"] = uint32(info->gpuCompressType);

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
    info.bGpuCompress = textureMetadata["bGpuCompress"];
    info.gpuCompressType = EBlockType(textureMetadata["gpuCompressType"]);

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

bool bakeTexture(const char* pathIn,const char* pathOut,bool srgb,bool compress,uint32 req_comp,bool bGenerateMipmap,bool bGpuCompress,EBlockType blockType)
{
    CHECK(req_comp == 4);
    int32 texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(pathIn, &texWidth, &texHeight, &texChannels, req_comp);

    if (!pixels) 
    {
        LOG_IO_FATAL("Fail to load image {0}.",pathIn);
    }

    static const auto BLOCK_TYPE = EBlockType::BC3;

	std::vector<unsigned char> generateMips{};
    bool bGenerateMipsSucess = false;
	if(bGenerateMipmap)
	{
        bGenerateMipsSucess = mipmapGenerate((unsigned char*)pixels,generateMips,texWidth,texHeight,req_comp,srgb);
	}

    const bool bNeedGpuCompress = bGpuCompress && bGenerateMipsSucess;
    std::vector<unsigned char> generateMipsCompressGpu{};
    bool bGpuCompressSucess = false;
    if(bNeedGpuCompress)
    {
        bGpuCompressSucess = mipmapCompressGpu(generateMips,generateMipsCompressGpu,texWidth,texHeight,req_comp,srgb,BLOCK_TYPE);
    }

    TextureInfo info;
    info.bCacheMipmaps = bGenerateMipsSucess;
    info.gpuCompressType = blockType;
    info.bGpuCompress = bGpuCompressSucess;
    info.mipmapLevels = getMipLevelsCount(texWidth,texHeight);

    int32 imageSize = 0;
    if(bGpuCompressSucess)
    {
        CHECK(getTextureMipmapPixelCountGpuCompress(texWidth,texHeight,info.mipmapLevels,BLOCK_TYPE) == (uint32)generateMipsCompressGpu.size());
        imageSize = (int32)generateMipsCompressGpu.size();
    }
    else if(bGenerateMipsSucess)
    {
        CHECK(getTextureMipmapPixelCount(texWidth,texHeight,req_comp,info.mipmapLevels) == (uint32)generateMips.size());
        imageSize = (int32)generateMips.size();
    }
    else
    {
        imageSize = texWidth * texHeight * req_comp;
    }

    
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

    AssetFile newImage{};
    if(bGpuCompressSucess)
    {
        newImage = packTexture(&info,generateMipsCompressGpu.data());
    }
    else if(bGenerateMipsSucess)
    {
        newImage = packTexture(&info,generateMips.data());
    }
    else
    {
        newImage = packTexture(&info,pixels);
    }
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
            if(this->bGpuCompress)
            {
                if(this->gpuCompressType==EBlockType::BC3)
                {
                    return VK_FORMAT_BC3_SRGB_BLOCK;
                }
            }
            else
            {
                return VK_FORMAT_R8G8B8A8_UNORM;
            }
        }
        else
        {
            if(this->bGpuCompress)
            {
                if(this->gpuCompressType==EBlockType::BC3)
                {
                    return VK_FORMAT_BC3_UNORM_BLOCK;
                }
            }
            else
            {
                return VK_FORMAT_R8G8B8A8_UNORM;
            }
        }
    }
    }
    LOG_FATAL("Unkonw format!");
}

inline void submitAsync(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkFence fence,
    const std::function<void(VkCommandBuffer cb)>& func)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device,&allocInfo,&commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer,&beginInfo);
    func(commandBuffer);

    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(queue,1,&submitInfo,fence);
    vkFreeCommandBuffers(device,commandPool,1,&commandBuffer);
}

namespace asset_system
{
    // NOTE: 一个Uploader对应一个纹理上传任务
    class GpuUploadTextureAsync
    {
        bool m_ready = false;
        VkCommandPool m_pool = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;

        void release()
        {
            if(fence!=nullptr)
            {
                VulkanRHI::get()->getFencePool().waitAndReleaseFence(fence,1000000);
                fence = nullptr;
            }
            if(uploadBuffers.size()>0)
            {
                for(auto* buf : uploadBuffers)
                {
                    delete buf;
                    buf = nullptr;
                }
                uploadBuffers.clear();
                uploadBuffers.resize(0);
            }
            finishCallBack = {};
            if(cmdBuf!=VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(m_device,m_pool,1,&cmdBuf);
                cmdBuf = VK_NULL_HANDLE;
            }
        }
    public:
        Ref<VulkanFence> fence = nullptr;
        std::vector<VulkanBuffer*> uploadBuffers{};
        std::function<void()> finishCallBack{};
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

        void init()
        {
            release();
            m_device = *VulkanRHI::get()->getVulkanDevice();
            m_pool = VulkanRHI::get()->getCopyCommandPool();

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = VulkanRHI::get()->getCopyCommandPool();
            allocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(*VulkanRHI::get()->getVulkanDevice(),&allocInfo,&cmdBuf);

            fence = VulkanRHI::get()->getFencePool().createFence(false);
        }

        void beginCmd()
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmdBuf,&beginInfo);
        }

        void endCmdAndSubmit()
        {
            vkEndCommandBuffer(cmdBuf);
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuf;

            vkQueueSubmit(VulkanRHI::get()->getAsyncCopyQueue()->queue,1,&submitInfo,fence->getFence());
        }

        bool tick()
        {
            if(!m_ready)
            {
                if(VulkanRHI::get()->getFencePool().isFenceSignaled(fence))
                {
                    m_ready = true;
                    if(finishCallBack)
                    {
                        finishCallBack();
                    }
                    release();
                }
            }
            return m_ready;
        }
    };
}

void asset_system::AssetSystem::checkTextureUploadStateAsync()
{
    for(auto iterP = m_uploadingTextureAsyncTask.begin(); iterP!=m_uploadingTextureAsyncTask.end();)
    {
        bool tickResult = (*iterP)->tick();
        if(tickResult) // finish
        {
            iterP = m_uploadingTextureAsyncTask.erase(iterP);
        }
        else
        {
            ++iterP;
        }
    }
}

VkSampler toVkSampler(asset_system::ESamplerType type)
{
    switch(type)
    {
    case engine::asset_system::ESamplerType::PointClamp:
        return VulkanRHI::get()->getPointClampEdgeSampler();

    case engine::asset_system::ESamplerType::PointRepeat:
        return VulkanRHI::get()->getPointRepeatSampler();

    case engine::asset_system::ESamplerType::LinearClamp:
        return VulkanRHI::get()->getLinearClampSampler();

    case engine::asset_system::ESamplerType::LinearRepeat:
        return VulkanRHI::get()->getLinearRepeatSampler();
    }
    return VulkanRHI::get()->getPointClampEdgeSampler();
}


bool asset_system::AssetSystem::loadTexture2DImage(CombineTexture& inout,const std::string& gameName)
{
    using namespace asset_system;
    if(std::filesystem::exists(gameName)) // 文件若不存在则返回fasle并换成替代纹理
    {
        CHECK(inout.texture == nullptr);

        AssetFile asset {};
        loadBinFile(gameName.c_str(),asset);
        CHECK(asset.type[0]=='T'&&
            asset.type[1]=='E'&&
            asset.type[2]=='X'&&
            asset.type[3]=='I');

        TextureInfo info = readTextureInfo(&asset);

        // Unpack data to container.
        std::vector<uint8> pixelData{};
        pixelData.resize(info.textureSize);
        unpackTexture(&info,asset.binBlob.data(),asset.binBlob.size(),(char*)pixelData.data());
        inout.sampler = toVkSampler(info.samplerType);

        if(info.bCacheMipmaps)
        {
            inout.texture = Texture2DImage::create(
                VulkanRHI::get()->getVulkanDevice(),
                info.pixelSize[0],
                info.pixelSize[1],
                VK_IMAGE_ASPECT_COLOR_BIT,
                info.getVkFormat(),
                false,
                info.mipmapLevels
            );

            uint32 mipWidth  = info.pixelSize[0];
            uint32 mipHeight = info.pixelSize[1];
            uint32 offsetPtr = 0; 

            auto uploader = std::make_shared<GpuUploadTextureAsync>();
            m_uploadingTextureAsyncTask.push_back(uploader);
            uploader->init();

            const bool bGpuCompress = info.bGpuCompress;
            const auto blockType = info.gpuCompressType;

            // 首先填充stageBuffer
            for(uint32 level = 0; level<info.mipmapLevels; level++)
            {
                CHECK(mipWidth >= 1 && mipHeight >= 1);
                uint32 currentMipLevelSize = mipWidth * mipHeight * TEXTURE_COMPONENT;
                if(bGpuCompress)
                {
                    currentMipLevelSize = getTotoalBlockPixelCount(blockType,mipWidth,mipHeight);
                }

                auto* stageBuffer = VulkanBuffer::create(
                    VulkanRHI::get()->getVulkanDevice(),
                    VulkanRHI::get()->getCopyCommandPool(),
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                    VMA_MEMORY_USAGE_CPU_TO_GPU,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    (VkDeviceSize)currentMipLevelSize,
                    (void*)(pixelData.data() + offsetPtr)
                );
                uploader->uploadBuffers.push_back(stageBuffer);
                mipWidth  /= 2;
                mipHeight /= 2;
                offsetPtr += currentMipLevelSize;
            }

            const uint32 mipLevels = info.mipmapLevels;
            mipWidth = info.pixelSize[0]; mipHeight = info.pixelSize[1];
            VulkanImage* imageProcess = inout.texture;

            uploader->beginCmd();
            for(uint32 level = 0; level < mipLevels; level++)
            {
                CHECK(mipWidth >= 1 && mipHeight >= 1);
                imageProcess->copyBufferToImage(uploader->cmdBuf,*uploader->uploadBuffers[level],mipWidth,mipHeight,VK_IMAGE_ASPECT_COLOR_BIT,level);
                if(level==(mipLevels-1))
                {
                    imageProcess->transitionLayout(uploader->cmdBuf,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_ASPECT_COLOR_BIT);
                }
                mipWidth  /= 2;
                mipHeight /= 2;
            }
            uploader->endCmdAndSubmit();

            uploader->finishCallBack = [&inout]()
            {
                inout.bReady = true;
                TextureLibrary::get()->updateTextureToBindlessDescriptorSet(inout);
            };
        }
        else
        {
            LOG_GRAPHICS_INFO("Uploading texture {0} to GPU and generate mipmaps...",gameName);
            inout.texture = Texture2DImage::createAndUpload(
                VulkanRHI::get()->getVulkanDevice(),
                VulkanRHI::get()->getGraphicsCommandPool(),
                VulkanRHI::get()->getGraphicsQueue(),
                pixelData,
                info.pixelSize[0],
                info.pixelSize[1],
                VK_IMAGE_ASPECT_COLOR_BIT,
                info.getVkFormat()
            );
            inout.bReady = true;
            TextureLibrary::get()->updateTextureToBindlessDescriptorSet(inout);
        }
        return true;
    }

    return false;
}

Texture2DImage* asset_system::loadFromFile(const std::string& path,VkFormat format,uint32 req,bool flip,bool bGenMipmaps)
{
    int32 texWidth, texHeight, texChannels;
    stbi_set_flip_vertically_on_load(flip);  
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,req);

    if (!pixels) 
    {
        LOG_IO_FATAL("Fail to load image {0}.",path);
    }

    int32 imageSize = texWidth * texHeight * req;

    std::vector<uint8> pixelData{};
    pixelData.resize(imageSize);

    memcpy(pixelData.data(),pixels,imageSize);

    stbi_image_free(pixels);
    stbi_set_flip_vertically_on_load(false);  

    Texture2DImage* ret = Texture2DImage::createAndUpload(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VulkanRHI::get()->getGraphicsQueue(),
        pixelData,
        texWidth,
        texHeight,
        VK_IMAGE_ASPECT_COLOR_BIT,
        format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        false,
        bGenMipmaps ? -1 : 1
    );

    return ret;
}

Texture2DImage* loadFromFileHdr(const std::string& path,VkFormat format,uint32 req,bool flip,bool bGenMipmaps = true)
{
    int32 texWidth, texHeight, texChannels;
    stbi_set_flip_vertically_on_load(flip);  

    float* pixels = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels,req);

    if (!pixels) 
    {
        LOG_IO_FATAL("Fail to load image {0}.",path);
    }

    int32 imageSize = texWidth * texHeight * req * 4;


    std::vector<uint8> pixelData{};
    pixelData.resize(imageSize);

    memcpy(pixelData.data(),reinterpret_cast<uint8*>(pixels),imageSize);

    stbi_image_free(pixels);
    stbi_set_flip_vertically_on_load(false);  

    Texture2DImage* ret = Texture2DImage::createAndUpload(
        VulkanRHI::get()->getVulkanDevice(),
        VulkanRHI::get()->getGraphicsCommandPool(),
        VulkanRHI::get()->getGraphicsQueue(),
        pixelData,
        texWidth,
        texHeight,
        VK_IMAGE_ASPECT_COLOR_BIT,
        format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        false,
        bGenMipmaps ? -1 : 1
    );

    return ret;
}

void asset_system::EngineAsset::init()
{
    if(bInit) return;
    std::string mediaDir = s_mediaDir; 

    iconFolder = new IconInfo(mediaDir + "icon/folder.png");
    iconFile = new IconInfo(mediaDir + "icon/file.png");
    iconBack = new IconInfo(mediaDir + "icon/back.png");
    iconHome = new IconInfo(mediaDir + "icon/home.png");
    iconFlash = new IconInfo(mediaDir + "icon/flash.png");
    iconProject = new IconInfo(mediaDir + "icon/project.png");
    iconMaterial = new IconInfo(mediaDir + "icon/material.png");
    iconMesh = new IconInfo(mediaDir + "icon/mesh.png");
    iconTexture = new IconInfo(mediaDir + "icon/image.png");

    bInit = true;
}

void asset_system::EngineAsset::release()
{
    if(!bInit) return;

    delete iconFolder;
    delete iconFile;
    delete iconBack;
    delete iconHome;
    delete iconFlash;
    delete iconProject;
    delete iconMesh;
    delete iconMaterial;
    delete iconTexture;
    bInit = false;
}

asset_system::EngineAsset::IconInfo* asset_system::EngineAsset::getIconInfo(const std::string& name)
{
    if(m_cacheIconInfo[name].icon == nullptr)
    {
        if(TextureLibrary::get()->existTexture(name))
        {
            auto pair = TextureLibrary::get()->getCombineTextureByName(name);
            if(pair.first == ERequestTextureResult::Ready)
            {
                m_cacheIconInfo[name].icon = pair.second.texture;
                m_cacheIconInfo[name].sampler = pair.second.sampler;
            }
        }
    }
    return &m_cacheIconInfo[name];
}

void asset_system::AssetSystem::loadEngineTextures()
{
    auto* textureLibrary = TextureLibrary::get();

    textureLibrary->m_textureContainer[s_defaultWhiteTextureName] = {};
    auto& whiteTexture = textureLibrary->m_textureContainer[s_defaultWhiteTextureName];
    whiteTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
    whiteTexture.texture = loadFromFile(s_defaultWhiteTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
    whiteTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(whiteTexture);

    textureLibrary->m_textureContainer[s_defaultBlackTextureName] = {};
    auto& blackTexture = textureLibrary->m_textureContainer[s_defaultBlackTextureName];
    blackTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
    blackTexture.texture = loadFromFile(s_defaultBlackTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
    blackTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(blackTexture);

    textureLibrary->m_textureContainer[s_defaultNormalTextureName] = {};
    auto& normalTexture = textureLibrary->m_textureContainer[s_defaultNormalTextureName];
    normalTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
    normalTexture.texture = loadFromFile(s_defaultNormalTextureName,VK_FORMAT_B8G8R8A8_UNORM,4,false);
    normalTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(normalTexture);

    textureLibrary->m_textureContainer[s_defaultCheckboardTextureName] = {};
    auto& checkboxTexture = textureLibrary->m_textureContainer[s_defaultCheckboardTextureName];
    checkboxTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
    checkboxTexture.texture = loadFromFile(s_defaultCheckboardTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);
    checkboxTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(checkboxTexture);

    textureLibrary->m_textureContainer[s_defaultEmissiveTextureName] = {};
    auto& emissiveTexture = textureLibrary->m_textureContainer[s_defaultEmissiveTextureName];
    emissiveTexture.sampler = VulkanRHI::get()->getLinearRepeatSampler();
    emissiveTexture.texture = loadFromFile(s_defaultEmissiveTextureName,VK_FORMAT_R8G8B8A8_SRGB,4,false);
    emissiveTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(emissiveTexture);

    textureLibrary->m_textureContainer[s_defaultHdrEnvTextureName] = {};
    auto& hdrEnvTexture = textureLibrary->m_textureContainer[s_defaultHdrEnvTextureName];
    hdrEnvTexture.sampler = VulkanRHI::get()->getPointClampEdgeSampler();
    hdrEnvTexture.texture = loadFromFileHdr(s_defaultHdrEnvTextureName,VK_FORMAT_R32G32B32A32_SFLOAT,4,false,true);
    hdrEnvTexture.bReady = true;
    TextureLibrary::get()->updateTextureToBindlessDescriptorSet(hdrEnvTexture);
}


void asset_system::EngineAsset::IconInfo::init(const std::string& path,bool flip)
{
    icon = loadFromFile(path, VK_FORMAT_R8G8B8A8_SRGB, 4,flip);
    sampler = VulkanRHI::get()->getLinearClampSampler();
}

void asset_system::EngineAsset::IconInfo::release()
{
    delete icon;
}

void* asset_system::EngineAsset::IconInfo::getId()
{
    if(cacheId!=nullptr)
    {
        return cacheId;
    }
    else
    {
        cacheId = (void*)ImGui_ImplVulkan_AddTexture(sampler,icon->getImageView(),icon->getCurentLayout());
        return cacheId;
    }
}

uint32 asset_system::getBlockSize(EBlockType fmt)
{
    switch(fmt)
    {
    case engine::asset_system::EBlockType::BC3:
        return 16;
    }

    LOG_FATAL("UNKONW BLOCK TYPE!");
    return 0;
}

uint32 engine::asset_system::getTotoalBlockPixelCount(EBlockType fmt,uint32 width,uint32 height)
{
    CHECK(width == height);

    if(fmt == EBlockType::BC3)
    {
        uint32 trueWidth  = width  >= 4 ? width  : 4;
        uint32 trueHeight = height >= 4 ? height : 4;

        // NOTE:BC3 4通道 1/4压缩
        return trueWidth * trueHeight;// * 4 / 4;
    }

    LOG_FATAL("ERROR Block Type!");
    return 0;
}

}