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

template <typename T>
struct TIsIntegral
{
	enum { Value = false };
};

template <> struct TIsIntegral<         bool>      { enum { Value = true }; };
template <> struct TIsIntegral<         char>      { enum { Value = true }; };
template <> struct TIsIntegral<signed   char>      { enum { Value = true }; };
template <> struct TIsIntegral<unsigned char>      { enum { Value = true }; };
template <> struct TIsIntegral<         char16_t>  { enum { Value = true }; };
template <> struct TIsIntegral<         char32_t>  { enum { Value = true }; };
template <> struct TIsIntegral<         wchar_t>   { enum { Value = true }; };
template <> struct TIsIntegral<         short>     { enum { Value = true }; };
template <> struct TIsIntegral<unsigned short>     { enum { Value = true }; };
template <> struct TIsIntegral<         int>       { enum { Value = true }; };
template <> struct TIsIntegral<unsigned int>       { enum { Value = true }; };
template <> struct TIsIntegral<         long>      { enum { Value = true }; };
template <> struct TIsIntegral<unsigned long>      { enum { Value = true }; };
template <> struct TIsIntegral<         long long> { enum { Value = true }; };
template <> struct TIsIntegral<unsigned long long> { enum { Value = true }; };

template <typename T> struct TIsIntegral<const          T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<      volatile T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<const volatile T> { enum { Value = TIsIntegral<T>::Value }; };

template <typename T>
struct TIsPointer
{
	enum { Value = false };
};

template <typename T> struct TIsPointer<T*> { enum { Value = true }; };

template <typename T> struct TIsPointer<const          T> { enum { Value = TIsPointer<T>::Value }; };
template <typename T> struct TIsPointer<      volatile T> { enum { Value = TIsPointer<T>::Value }; };
template <typename T> struct TIsPointer<const volatile T> { enum { Value = TIsPointer<T>::Value }; };

template <typename T>
inline constexpr T align(T Val, uint64 Alignment)
{
	static_assert(TIsIntegral<T>::Value || TIsPointer<T>::Value, "Align expects an integer or pointer type");
	return (T)(((uint64)Val + Alignment - 1) & ~(Alignment - 1));
}

template <bool Predicate, typename Result = void>
class TEnableIf;

template <typename Result>
class TEnableIf<true, Result>
{
public:
	using type = Result;
	using Type = Result;
};

template <typename Result>
class TEnableIf<false, Result>
{ };

}


namespace glm
{
	template<class Archive> void serialize(Archive& archive, glm::vec2& v)  { archive(v.x, v.y); }
	template<class Archive> void serialize(Archive& archive, glm::vec3& v)  { archive(v.x, v.y, v.z); }
	template<class Archive> void serialize(Archive& archive, glm::vec4& v)  { archive(v.x, v.y, v.z, v.w); }
	template<class Archive> void serialize(Archive& archive, glm::ivec2& v) { archive(v.x, v.y); }
	template<class Archive> void serialize(Archive& archive, glm::ivec3& v) { archive(v.x, v.y, v.z); }
	template<class Archive> void serialize(Archive& archive, glm::ivec4& v) { archive(v.x, v.y, v.z, v.w); }
	template<class Archive> void serialize(Archive& archive, glm::uvec2& v) { archive(v.x, v.y); }
	template<class Archive> void serialize(Archive& archive, glm::uvec3& v) { archive(v.x, v.y, v.z); }
	template<class Archive> void serialize(Archive& archive, glm::uvec4& v) { archive(v.x, v.y, v.z, v.w); }
	template<class Archive> void serialize(Archive& archive, glm::dvec2& v) { archive(v.x, v.y); }
	template<class Archive> void serialize(Archive& archive, glm::dvec3& v) { archive(v.x, v.y, v.z); }
	template<class Archive> void serialize(Archive& archive, glm::dvec4& v) { archive(v.x, v.y, v.z, v.w); }

	// glm matrices serialization
	template<class Archive> void serialize(Archive& archive, glm::mat2& m)  { archive(m[0], m[1]); }
	template<class Archive> void serialize(Archive& archive, glm::dmat2& m) { archive(m[0], m[1]); }
	template<class Archive> void serialize(Archive& archive, glm::mat3& m)  { archive(m[0], m[1], m[2]); }
	template<class Archive> void serialize(Archive& archive, glm::mat4& m)  { archive(m[0], m[1], m[2], m[3]); }
	template<class Archive> void serialize(Archive& archive, glm::dmat4& m) { archive(m[0], m[1], m[2], m[3]); }

	template<class Archive> void serialize(Archive& archive, glm::quat& q)  { archive(q.x, q.y, q.z, q.w); }
	template<class Archive> void serialize(Archive& archive, glm::dquat& q) { archive(q.x, q.y, q.z, q.w); }
}
