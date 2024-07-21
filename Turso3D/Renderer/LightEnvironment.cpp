#include <Turso3D/Renderer/LightEnvironment.h>

namespace Turso3D
{
	LightEnvironment::LightEnvironment() :
		ambientColor(Color {0.0f, 0.0f, 0.0f}),
		fogColor(Color {0.0f, 0.0f, 0.0f}),
		fogStart(100.0f),
		fogEnd(1000.0f)
	{
	}

	LightEnvironment::~LightEnvironment()
	{
	}

	void LightEnvironment::SetIBLMaps(std::shared_ptr<Texture> brdf, std::shared_ptr<Texture> iem, std::shared_ptr<Texture> pmrem)
	{
		brdfTex = brdf;
		iemTex = iem;
		pmremTex = pmrem;
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
