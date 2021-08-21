#pragma once
#include "../core/noncopyable.h"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * NOTE: 简单的RenderGraph实现，用于简化RenderPass的组织代码
 * TODO:
**/

namespace engine{ namespace render_graph{

class RenderGraph : NonCopyable
{
private:


public:
	RenderGraph();
	~RenderGraph();

};

}}