#pragma once

#include <Turso3D/Math/Color.h>
#include <Turso3D/Scene/Node.h>

namespace Turso3D
{
	constexpr float DEFAULT_FOG_START = 500.0f;
	constexpr float DEFAULT_FOG_END = 1000.0f;

	static const Color DEFAULT_AMBIENT_COLOR(0.1f, 0.1f, 0.1f);
	static const Color DEFAULT_FOG_COLOR(Color::BLACK);

	// Global lighting settings.
	// Should be created as a child of the scene root.
	class LightEnvironment : public Node
	{
	public:
		// Construct.
		LightEnvironment();
		// Destruct.
		~LightEnvironment();

		// Set ambient light color.
		void SetAmbientColor(const Color& color);
		// Set fog end color.
		void SetFogColor(const Color& color);
		// Set fog start distance.
		void SetFogStart(float distance);
		// Set fog end distance.
		void SetFogEnd(float distance);

		// Return ambient light color.
		const Color& AmbientColor() const { return ambientColor; }
		// Return fog end color.
		const Color& FogColor() const { return fogColor; }
		// Return fog start distance.
		float FogStart() const { return fogStart; }
		// Return fog end distance.
		float FogEnd() const { return fogEnd; }

	private:
		// Ambient light color.
		Color ambientColor;
		// Fog end color.
		Color fogColor;
		// Fog start distance.
		float fogStart;
		// Fog end distance.
		float fogEnd;
	};
}
