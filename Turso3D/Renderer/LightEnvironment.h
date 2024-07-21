#pragma once

#include <Turso3D/Math/Color.h>
#include <memory>

namespace Turso3D
{
	class Texture;

	// Global lighting settings.
	class LightEnvironment
	{
	public:
		// Construct.
		LightEnvironment();
		// Destruct.
		~LightEnvironment();

		// Set the textures for IBL lighting.
		void SetIBLMaps(std::shared_ptr<Texture> brdf, std::shared_ptr<Texture> iem, std::shared_ptr<Texture> pmrem);

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

		Texture* GetIEMTexture() const { return iemTex.get(); }
		Texture* GetPMREMTexture() const { return pmremTex.get(); }
		Texture* GetBRDFTexture() const { return brdfTex.get(); }

	private:
		// Ambient light color.
		Color ambientColor;
		// Fog end color.
		Color fogColor;
		// Fog start distance.
		float fogStart;
		// Fog end distance.
		float fogEnd;

		// BRDF LUT map
		std::shared_ptr<Texture> brdfTex;
		// Irradiance Environment Map
		std::shared_ptr<Texture> iemTex;
		// Prefiltered Mipmaped Radiance Environment Map
		std::shared_ptr<Texture> pmremTex;
	};
}
