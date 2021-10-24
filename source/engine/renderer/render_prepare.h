#pragma once
#include <vector>
#include "../core/core.h"

namespace engine{

struct RenderMesh;
struct GPUFrameData;

extern void frustumCulling(std::vector<RenderMesh>& inMeshes,const GPUFrameData& view);

}