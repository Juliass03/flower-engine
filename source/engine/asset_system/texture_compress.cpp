#include "texture_compress.h"

using namespace engine;
using namespace engine::asset_system;

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

		// NOTE:BC3 4Í¨µÀ 1/4Ñ¹Ëõ
		return trueWidth * trueHeight;// * 4 / 4;
	}

	LOG_FATAL("ERROR Block Type!");
	return 0;
}
