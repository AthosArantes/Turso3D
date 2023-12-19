#include "Log.h"
#include <cassert>
#include <ctime>
#include <mutex>
#include <fstream>
#include <deque>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <SDKDDKVer.h>
	#include <Windows.h>
#endif

namespace
{
	constexpr std::string_view LogLevelPrefixes[] =
	{
		"",
		"[TRACE] ",
		"[DEBUG] ",
		"[INFO] ",
		"[WARNING] ",
		"[ERROR] "
	};

	static std::mutex logMutex {};
	static std::fstream logFile {};

	static thread_local std::deque<std::string> scopes {};
}

namespace Turso3D
{
	Log::Scope::Scope(const std::string& name)
	{
		scopes.push_back(name);
	}
	Log::Scope::Scope(const char* name)
	{
		scopes.push_back(std::string {name});
	}
	Log::Scope::~Scope()
	{
		assert(scopes.size());
		scopes.pop_back();
	}

	// ==========================================================================================
	void Log::Initialize(const std::string& filepath, bool truncate)
	{
		int flags = std::ios::binary | std::ios::in | std::ios::out;
		if (truncate) {
			flags |= std::ios::trunc;
		}
		logFile.open(filepath, flags);

		// Seek to end if not truncating
		if (!truncate) {
			logFile.seekg(0, std::ios::end);
		}
	}

	void Log::Write(LogLevel level, std::string_view message, bool timestamp)
	{
		// Format the message
		std::string output;
		if (timestamp) {
			char buffer[64] {};
			time_t sysTime = time(nullptr);

			strftime(buffer, std::size(buffer), "[%Y-%m-%d %H:%M:%S] ", localtime(&sysTime));
			output.append(buffer);
		}

#ifdef _WIN32
		HANDLE handle = GetCurrentThread();

		PWSTR tDesc;
		HRESULT hr = GetThreadDescription(handle, &tDesc);
		if (SUCCEEDED(hr)) {
			int sz = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, tDesc, -1, nullptr, 0, NULL, NULL);

			std::string str;
			str.resize(sz);
			sz = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, tDesc, -1, str.data(), str.size(), NULL, NULL);
			str.resize(sz - 1);

			output.append("[");
			output.append(str);
			output.append("] ");
		}
#endif

		if (scopes.size()) {
			output.append("[");
			output.append(scopes.back());
			output.append("] ");
		}

		size_t intLevel = static_cast<size_t>(level);
		if (level != LogLevel::None && intLevel < std::size(LogLevelPrefixes)) {
			output.append(LogLevelPrefixes[intLevel]);
		}
		output.append(message);
		output.append("\n");

		{
			std::lock_guard lock(logMutex);
			logFile.write(output.data(), output.size());
			logFile.flush();
		}
	}
}
