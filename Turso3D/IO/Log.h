#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>

#define TURSO3D_LOG_RAW_ENABLED
#define TURSO3D_LOG_TRACE_ENABLED
#define TURSO3D_LOG_DEBUG_ENABLED

namespace Turso3D
{
	enum class LogLevel
	{
		None,
		Trace,
		Debug,
		Info,
		Warning,
		Error
	};

	namespace Log
	{
		// Initialize and open the log file.
		void Initialize(const std::string& filepath, bool truncate);
		// Write to the log.
		void Write(bool timestamp, LogLevel level, std::string_view message);

		// Write formatted string to the log.
		template <typename... Args>
		inline void Write(bool timestamp, LogLevel level, std::string_view format, Args&&... args)
		{
			Write(timestamp, level, fmt::template format(format, std::forward<Args>(args)...));
		}
	}

#ifdef TURSO3D_LOG_RAW_ENABLED
	template <typename... Args>
	inline void LOG_RAW(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::None, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_RAW(...)
#endif

#ifdef TURSO3D_LOG_TRACE_ENABLED
	template <typename... Args>
	inline void LOG_TRACE(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::Trace, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_TRACE(...)
#endif

#ifdef TURSO3D_LOG_DEBUG_ENABLED
	template <typename... Args>
	inline void LOG_DEBUG(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::Debug, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_DEBUG(...)
#endif

	template <typename... Args>
	inline void LOG_INFO(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::Info, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_WARNING(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::Warning, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_ERROR(std::string_view format, Args&&... args)
	{
		Log::template Write(true, LogLevel::Error, format, std::forward<Args>(args)...);
	}
}
