#pragma once
#include <memory.h>
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#pragma warning(pop)
#include <mutex>
#include "noncopyable.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <deque>

namespace engine{

enum class ELogType : uint8_t
{
	trace,
	info,
	warn,
	error,
	fatal
};

template<typename Mutex>
class LogCacheSink : public spdlog::sinks::base_sink <Mutex>
{
private:
	std::vector<std::function<void(std::string,ELogType)>> m_callbacks;

	static ELogType toLogType(spdlog::level::level_enum level)
	{
		switch(level)
		{
		case spdlog::level::info:
			return ELogType::info;
		case spdlog::level::warn:
			return ELogType::warn;
		case spdlog::level::err:
			return ELogType::error;
		case spdlog::level::critical:
			return ELogType::fatal;
		default:
			return ELogType::trace;
		}
	}

protected:
	// 此处的写入已经加锁
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
		for(auto& fn : m_callbacks)
		{
			fn(fmt::to_string(formatted), toLogType(msg.level));
		}
	}

	void flush_() override 
	{

	}

public:
	void pushCallback(std::function<void(std::string,ELogType)> callback)
	{
		m_callbacks.push_back(callback);
	}
};

class Logger : private NonCopyable
{
private:
	static  std::shared_ptr<Logger> s_instance;
	std::shared_ptr<spdlog::logger> m_loggerUtil;
	std::shared_ptr<spdlog::logger> m_loggerIO;
	std::shared_ptr<spdlog::logger> m_loggerGraphics;
	void init();
	std::shared_ptr<LogCacheSink<std::mutex>> m_logCache;

public:
	Logger();
	inline static std::shared_ptr<Logger>& getInstance()
	{
		return s_instance;
	}

	inline std::shared_ptr<spdlog::logger>& getLoggerUtil()
	{
		return m_loggerUtil;
	}

	inline std::shared_ptr<spdlog::logger>& getLoggerIo()
	{
		return m_loggerIO;
	}

	inline std::shared_ptr<spdlog::logger>& getLoggerGraphics()
	{
		return m_loggerGraphics;
	}

	auto& getLogCache()
	{
		return m_logCache;
	}
};

}