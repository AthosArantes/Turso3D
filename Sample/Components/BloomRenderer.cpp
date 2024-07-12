#include "BloomRenderer.h"
#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

BloomRenderer::BloomRenderer()
{
	blurRenderer = std::make_unique<BlurRenderer>();
	buffer = std::make_unique<Texture>();
	fbo = std::make_unique<FrameBuffer>();
}

BloomRenderer::~BloomRenderer()
{
}

void BloomRenderer::Initialize()
{
	blurRenderer->Initialize();

	constexpr StringHash intensityHash {"intensity"};

	bloomProgram = Graphics::CreateProgram("PostProcess/BloomCompose.glsl", "", "");
	uIntensity = bloomProgram->Uniform(intensityHash);
}

void BloomRenderer::UpdateBuffers(const IntVector2& size, ImageFormat format)
{
	Log::Scope logScope {"BloomRenderer::UpdateBuffers"};

	buffer->Define(TARGET_2D, size, format);
	buffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	fbo->Define(buffer.get(), nullptr);

	blurRenderer->UpdateBuffers(size / 2, format, IntVector2 {4, 4}, 0);
}

void BloomRenderer::Render(Texture* hdrColor, float intensity)
{
	Graphics::SetRenderState(BLEND_REPLACE, CULL_BACK, CMP_ALWAYS, true, false);
	blurRenderer->Downsample(hdrColor);

	Graphics::SetRenderState(BLEND_ADD, CULL_BACK, CMP_ALWAYS, true, false);
	blurRenderer->Upsample();

	// Compose
	Graphics::BindFramebuffer(fbo.get(), nullptr);

	Graphics::BindProgram(bloomProgram.get());
	Graphics::SetUniform(uIntensity, intensity);
	Graphics::BindTexture(0, hdrColor);
	Graphics::BindTexture(1, blurRenderer->GetTexture());

	Graphics::SetViewport(IntRect {IntVector2::ZERO(), buffer->Size2D()});
	Graphics::SetRenderState(BLEND_REPLACE, CULL_BACK, CMP_ALWAYS, true, false);
	Graphics::DrawQuad();
}
