#pragma once
#include <vector>
#include "../core/core.h"

namespace engine{

struct RenderMeshPack;
struct GPUFrameData;

extern void frustumCulling(RenderMeshPack& inMeshes,const GPUFrameData& view);

}