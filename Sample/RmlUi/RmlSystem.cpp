#include "RmlSystem.h"
#include <Turso3D/IO/Log.h>
#include <GLFW/glfw3.h>
#include <cassert>

using namespace Turso3D;

namespace
{
	static const char* RmlLogLevels[] = {"RAW", "ERROR", "ASSERT", "WARNING", "INFO", "DEBUG"};
}

// ==========================================================================================
double RmlSystem::GetElapsedTime()
{
	return glfwGetTime();
}

int RmlSystem::TranslateString(Rml::String& translated, const Rml::String& input)
{
	return SystemInterface::TranslateString(translated, input);
	// TODO
}

bool RmlSystem::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
	LOG_RAW("[RmlUi] [{:s}] {:s}", RmlLogLevels[type], message);
	if (type == Rml::Log::Type::LT_ASSERT) {
		assert(false);
	}
	return true;
}

void RmlSystem::SetMouseCursor(const Rml::String& cursor_name)
{
	SystemInterface::SetMouseCursor(cursor_name);
	// TODO
}

void RmlSystem::SetClipboardText(const Rml::String& text)
{
	SystemInterface::SetClipboardText(text);
	//glfwSetClipboardString()
}

void RmlSystem::GetClipboardText(Rml::String& text)
{
	SystemInterface::GetClipboardText(text);
	//glfwGetClipboardString()
}

#if 0
void RmlSystem::ActivateKeyboard(Rml::Vector2f caret_position, float line_height)
{
}

void RmlSystem::DeactivateKeyboard()
{
}
#endif

