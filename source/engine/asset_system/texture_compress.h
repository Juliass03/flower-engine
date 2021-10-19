#pragma once
#include "../core/core.h"

namespace engine{ namespace asset_system{

enum class EBlockType
{
    BC3 = 0,
};

extern uint32 getBlockSize(EBlockType fmt);

// 获取当前大小纹理的像素压缩后大小
extern uint32 getTotoalBlockPixelCount(EBlockType fmt,uint32 width,uint32 height);


}}