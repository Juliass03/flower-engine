#pragma once
#include "../../core/core.h"

namespace engine { namespace frame_graph{

enum class EQueueType : uint32_t
{
	Graphics,
	AsyncCompute,
	AsyncTransfer,

	Count,
	Unknown = ~0u,
};

enum class EQueueUsage : uint32_t
{
	Unknown = 0,
	Graphics      = 1 << uint32_t(EQueueType::Graphics),
	AsyncCompute  = 1 << uint32_t(EQueueType::AsyncCompute),
	AsyncTransfer = 1 << uint32_t(EQueueType::AsyncTransfer),

	Count,
	All = ((Count - 1) << 1) - 1,
};

} }