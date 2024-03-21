#include "BloomRenderer.h"
#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

namespace Turso3D
{
	BloomRenderer::BloomRenderer()
	{
		blurRenderer = std::make_unique<BlurRenderer>();

		resultTexture = std::make_unique<Texture>();
		resultFbo = std::make_unique<FrameBuffer>();
	}

	BloomRenderer::~BloomRenderer()
	{
	}

	void BloomRenderer::Initialize(Graphics* graphics_)
	{
		graphics = graphics_;

		programBrightness = graphics->CreateProgram("PostProcess/Brightness.glsl", "", "");
		uBrightThreshold = programBrightness->Uniform(StringHash {"threshold"});

		programCompose = graphics->CreateProgram("PostProcess/BloomCompose.glsl", "", "");
		uIntensity = programCompose->Uniform(StringHash {"intensity"});

		blurRenderer->Initialize(graphics);
	}

	void BloomRenderer::UpdateBuffers(const IntVector2& size)
	{
		Log::Scope logScope {"BloomRenderer::UpdateBuffers"};
		blurRenderer->UpdateBuffers(size, 0, FMT_R11_G11_B10F, false);

		resultTexture->Define(TEX_2D, size, FMT_R11_G11_B10F);
		resultTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		resultFbo->Define(resultTexture.get(), nullptr);
	}

	void BloomRenderer::Render(Texture* hdrColor, float brightThreshold, float intensity)
	{
		IntRect viewRect {0, 0, resultTexture->Width(), resultTexture->Height()};

		FrameBuffer* fbo = blurRenderer->GetResultFramebuffer();
		Texture* tex = blurRenderer->GetResultTexture();

		graphics->SetFrameBuffer(fbo);
		graphics->SetViewport(viewRect);

		// Sample bright areas
		programBrightness->Bind();
		glUniform1f(uBrightThreshold, brightThreshold);
		hdrColor->Bind(0);
		Texture::Unbind(1);
		Texture::Unbind(2);
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();

		// Perform blur
		blurRenderer->Render(tex);

		// Compose
		graphics->SetFrameBuffer(resultFbo.get());
		graphics->SetViewport(viewRect);

		programCompose->Bind();
		glUniform1f(uIntensity, intensity);
		hdrColor->Bind(0);
		tex->Bind(1);
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();
	}
}
