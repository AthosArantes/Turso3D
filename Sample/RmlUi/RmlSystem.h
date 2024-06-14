#pragma once

#include <RmlUi/Core/SystemInterface.h>

class RmlSystem : public Rml::SystemInterface
{
public:
	double GetElapsedTime() override;

	int TranslateString(Rml::String& translated, const Rml::String& input) override;

	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

	void SetMouseCursor(const Rml::String& cursor_name) override;

	void SetClipboardText(const Rml::String& text) override;
	void GetClipboardText(Rml::String& text) override;

	//void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
	//void DeactivateKeyboard() override;
};
