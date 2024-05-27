#include "BloomRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>

namespace Turso3D
{
	BloomRenderer::BloomRenderer()
	{
		resultTexture = std::make_unique<Texture>();
		resultFbo = std::make_unique<FrameBuffer>();
	}

	BloomRenderer::~BloomRenderer()
	{
	}

	void BloomRenderer::Initialize(Graphics* graphics_)
	{
		graphics = graphics_;

		constexpr StringHash intensityHash {"intensity"};

		bloomProgram = graphics->CreateProgram("PostProcess/BloomCompose.glsl", "", "");
		uIntensity = bloomProgram->Uniform(intensityHash);
	}

	void BloomRenderer::UpdateBuffers(const IntVector2& size, ImageFormat format)
	{
		Log::Scope logScope {"BloomRenderer::UpdateBuffers"};

		resultTexture->Define(TARGET_2D, size, format);
		resultTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		resultFbo->Define(resultTexture.get(), nullptr);

		blurPasses.clear();
		BlurRenderer::CreateMips(blurPasses, size / 2, format, IntVector2 {8, 8}, 0);
	}

	void BloomRenderer::Render(BlurRenderer* blurRenderer, Texture* hdrColor, float intensity)
	{
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);

		blurRenderer->Render(blurPasses, hdrColor);

		// Compose
		resultFbo->Bind();
		graphics->SetViewport(IntRect {IntVector2::ZERO(), resultTexture->Size2D()});

		bloomProgram->Bind();
		graphics->SetUniform(uIntensity, intensity);
		hdrColor->Bind(0);
		blurPasses[0].buffer->Bind(1);

		graphics->DrawQuad();
	}
}
