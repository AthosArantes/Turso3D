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
		class Scope
		{
			Scope() = delete;
			Scope(const Scope&) = delete;
			Scope(Scope&&) = delete;

			Scope& operator=(const Scope&) = delete;
			Scope& operator=(Scope&&) = delete;

		public:
			Scope(const std::string& name);
			Scope(const char* name);
			~Scope();
		};

		// Initialize and open the log file.
		void Initialize(const std::string& filepath, bool truncate);
		// Write to the log.
		void Write(LogLevel level, std::string_view message, bool timestamp = true);

		// Write formatted string to the log.
		template <typename... Args>
		inline void Write(LogLevel level, std::string_view format, Args&&... args)
		{
			Write(level, fmt::template format(format, std::forward<Args>(args)...), true);
		}

		// Return the string name of the calling thread
		std::string& ThreadName();
	}

#ifdef TURSO3D_LOG_RAW_ENABLED
	template <typename... Args>
	inline void LOG_RAW(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::None, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_RAW(...)
#endif

#ifdef TURSO3D_LOG_TRACE_ENABLED
	template <typename... Args>
	inline void LOG_TRACE(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::Trace, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_TRACE(...)
#endif

#ifdef TURSO3D_LOG_DEBUG_ENABLED
	template <typename... Args>
	inline void LOG_DEBUG(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::Debug, format, std::forward<Args>(args)...);
	}
#else
	#define LOG_DEBUG(...)
#endif

	template <typename... Args>
	inline void LOG_INFO(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::Info, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_WARNING(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::Warning, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_ERROR(std::string_view format, Args&&... args)
	{
		Log::template Write(LogLevel::Error, format, std::forward<Args>(args)...);
	}
}
