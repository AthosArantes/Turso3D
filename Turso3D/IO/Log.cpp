#include <Turso3D/IO/Log.h>
#include <cassert>
#include <ctime>
#include <mutex>
#include <fstream>
#include <vector>

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

	class ScopeManager
	{
	public:
		// Return the combined scopes as a string.
		static const std::string FormattedString()
		{
			std::vector<std::string>& scopes = Scopes();

			std::string output = "[";
			for (int i = 0; i < scopes.size(); ++i) {
				if (i != 0) {
					output.append("::");
				}
				output.append(scopes.at(i));
			}
			output.append("] ");

			return output;
		}

		static std::vector<std::string>& Scopes()
		{
			thread_local std::vector<std::string> scopes;
			return scopes;
		}
	};

	static std::mutex logMutex {};
	static std::fstream logFile {};
}

namespace Turso3D
{
	Log::Scope::Scope(const std::string& name)
	{
		ScopeManager::Scopes().push_back(name);
	}

	Log::Scope::Scope(const char* name)
	{
		ScopeManager::Scopes().push_back(std::string {name});
	}

	Log::Scope::~Scope()
	{
		assert(ScopeManager::Scopes().size());
		ScopeManager::Scopes().pop_back();
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

		const std::string& thread_name = ThreadName();
		if (!thread_name.empty()) {
			output.append("[");
			output.append(thread_name);
			output.append("] ");
		}

		size_t intLevel = static_cast<size_t>(level);
		if (level != LogLevel::None && intLevel < std::size(LogLevelPrefixes)) {
			output.append(LogLevelPrefixes[intLevel]);
		}

		if (!ScopeManager::Scopes().empty()) {
			output.append(ScopeManager::FormattedString());
		}

		output.append(message);
		output.append("\n");

		{
			std::lock_guard lock(logMutex);
			logFile.write(output.data(), output.size());
			logFile.flush();
		}
	}

	std::string& Log::ThreadName()
	{
		thread_local std::string name;
		return name;
	}
}
