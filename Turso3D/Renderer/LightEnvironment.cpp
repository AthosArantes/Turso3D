#include "LightEnvironment.h"

namespace Turso3D
{
	LightEnvironment::LightEnvironment() :
		ambientColor(DEFAULT_AMBIENT_COLOR),
		fogColor(DEFAULT_FOG_COLOR),
		fogStart(DEFAULT_FOG_START),
		fogEnd(DEFAULT_FOG_END)
	{
	}

	LightEnvironment::~LightEnvironment()
	{
	}

	void LightEnvironment::SetAmbientColor(const Color& color)
	{
		ambientColor = color;
	}

	void LightEnvironment::SetFogColor(const Color& color)
	{
		fogColor = color;
	}

	void LightEnvironment::SetFogStart(float distance)
	{
		fogStart = distance;
	}

	void LightEnvironment::SetFogEnd(float distance)
	{
		fogEnd = distance;
	}
}
