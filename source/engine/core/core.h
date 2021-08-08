#pragma once
#pragma warning (disable: 4996)
#pragma warning (disable: 4834)

#include "cvar.h"
#include "log.h"
#include "string.h"

// 开启Log打印
#define ENABLE_LOG

#ifdef ENABLE_LOG
#define LOG_TRACE(...) ::engine::Logger::getInstance()->getLoggerUtil()->trace(__VA_ARGS__)
#define LOG_INFO(...)  ::engine::Logger::getInstance()->getLoggerUtil()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::engine::Logger::getInstance()->getLoggerUtil()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::engine::Logger::getInstance()->getLoggerUtil()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::engine::Logger::getInstance()->getLoggerUtil()->critical(__VA_ARGS__);throw std::runtime_error("Utils Fatal!")
#define LOG_IO_TRACE(...) ::engine::Logger::getInstance()->getLoggerIo()->trace(__VA_ARGS__)
#define LOG_IO_INFO(...)  ::engine::Logger::getInstance()->getLoggerIo()->info(__VA_ARGS__)
#define LOG_IO_WARN(...)  ::engine::Logger::getInstance()->getLoggerIo()->warn(__VA_ARGS__)
#define LOG_IO_ERROR(...) ::engine::Logger::getInstance()->getLoggerIo()->error(__VA_ARGS__)
#define LOG_IO_FATAL(...) ::engine::Logger::getInstance()->getLoggerIo()->critical(__VA_ARGS__);throw std::runtime_error("IO Fatal!")
#define LOG_GRAPHICS_TRACE(...) ::engine::Logger::getInstance()->getLoggerGraphics()->trace(__VA_ARGS__)
#define LOG_GRAPHICS_INFO(...)  ::engine::Logger::getInstance()->getLoggerGraphics()->info(__VA_ARGS__)
#define LOG_GRAPHICS_WARN(...)  ::engine::Logger::getInstance()->getLoggerGraphics()->warn(__VA_ARGS__)
#define LOG_GRAPHICS_ERROR(...) ::engine::Logger::getInstance()->getLoggerGraphics()->error(__VA_ARGS__)
#define LOG_GRAPHICS_FATAL(...) ::engine::Logger::getInstance()->getLoggerGraphics()->critical(__VA_ARGS__);throw std::runtime_error("Graphics Fatal!")
#else
#define LOG_TRACE(...)   
#define LOG_INFO(...)    
#define LOG_WARN(...)   
#define LOG_ERROR(...)    
#define LOG_FATAL(...) throw std::runtime_error("UtilsFatal!")
#define LOG_IO_TRACE(...) 
#define LOG_IO_INFO(...)
#define LOG_IO_WARN(...)    
#define LOG_IO_ERROR(...)   
#define LOG_IO_FATAL(...) throw std::runtime_error("IOFatal!")
#define LOG_GRAPHICS_TRACE(...) 
#define LOG_GRAPHICS_INFO(...)
#define LOG_GRAPHICS_WARN(...)  
#define LOG_GRAPHICS_ERROR(...)   
#define LOG_GRAPHICS_FATAL(...) throw std::runtime_error("GraphicsFatal!")
#endif

#if defined(_DEBUG) || defined(DEBUG)
#define FLOWER_DEBUG
#endif

#if defined(FLOWER_DEBUG)
#define ASSERT(x, ...) { if(!(x)) { LOG_FATAL("Assert failed: {0}.", __VA_ARGS__); __debugbreak(); } }
#define CHECK(x) { if(!(x)) { LOG_FATAL("Check error."); __debugbreak(); } }
#else
#define ASSERT(x, ...) 
#define CHECK(x)
#endif

#include <vector>
#include <mutex>

// 强制弧度制
#define GLM_FORCE_RADIANS
// Vulkan深度 0 - 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// 开启实验性功能
#define GLM_ENABLE_EXPERIMENTAL
// 开启强制内存对齐
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace engine{

using int8   =   int8_t;
using int16  =  int16_t;
using int32  =  int32_t;
using int64  =  int64_t;
using uint8  =  uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

template<typename Worker> using Ref = Worker*;

}
