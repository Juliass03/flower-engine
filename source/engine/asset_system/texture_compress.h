#pragma once
#include "../core/core.h"

namespace engine{ namespace asset_system{

enum class EBlockType
{
    BC3 = 0,
};

extern uint32 getBlockSize(EBlockType fmt);

// ��ȡ��ǰ��С���������ѹ�����С
extern uint32 getTotoalBlockPixelCount(EBlockType fmt,uint32 width,uint32 height);


}}