#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

struct BlurRenderer::MipPass
{
	std::unique_ptr<Texture> buffer;
	std::unique_ptr<FrameBuffer> fbo;
};

// ==========================================================================================
BlurRenderer::BlurRenderer()
{
}

BlurRenderer::~BlurRenderer()
{
}

void BlurRenderer::Initialize()
{
	constexpr StringHash invSrcSizeHash {"invSrcSize"};
	constexpr StringHash filterRadiusHash {"filterRadius"};
	constexpr StringHash aspectRatioHash {"aspectRatio"};

	constexpr const char* downsampleDefines[] = {"FIRST_PASS", ""};
	for (int i = 0; i < 2; ++i) {
		downsampleProgram[i] = Graphics::CreateProgram("PostProcess/BlurDownsample.glsl", downsampleDefines[i], downsampleDefines[i]);
		uInvSrcSize[i] = downsampleProgram[i]->Uniform(invSrcSizeHash);
	}

	upsampleProgram = Graphics::CreateProgram("PostProcess/BlurUpsample.glsl", "", "");
	uFilterRadius = upsampleProgram->Uniform(filterRadiusHash);
	uAspectRatio = upsampleProgram->Uniform(aspectRatioHash);
}

void BlurRenderer::Downsample(Texture* srcColor)
{
	for (size_t i = 0; i < passes.size(); ++i) {
		// Bind the program
		int p = int(bool(i));
		Graphics::BindProgram(downsampleProgram[p].get());

		// Bind the draw buffer
		MipPass& mip = passes[i];
		Graphics::BindFramebuffer(mip.fbo.get(), nullptr);

		// Bind the texture to be sampled
		Texture* src = (i == 0) ? srcColor : passes[i - 1].buffer.get();
		Graphics::BindTexture(0, src);

		const IntVector2& src_size = src->Size2D();
		Graphics::SetViewport(IntRect {IntVector2::ZERO(), mip.buffer->Size2D()});
		Graphics::SetUniform(uInvSrcSize[p], Vector2 {1.0f / static_cast<float>(src_size.x), 1.0f / static_cast<float>(src_size.y)});
		Graphics::DrawQuad();
	}
}

void BlurRenderer::Upsample(float filterRadius)
{
	Graphics::BindProgram(upsampleProgram.get());
	Graphics::SetUniform(uAspectRatio, aspectRatio);
	Graphics::SetUniform(uFilterRadius, filterRadius);

	for (size_t i = 0; i < passes.size() - 1; ++i) {
		size_t ri = passes.size() - (1 + i);

		// Bind the draw buffer
		MipPass& prev_mip = passes[ri - 1];
		Graphics::BindFramebuffer(prev_mip.fbo.get(), nullptr);

		// Bind the mip texture to be sampled.
		MipPass& mip = passes[ri];
		Graphics::BindTexture(0, mip.buffer.get());

		Graphics::SetViewport(IntRect {IntVector2::ZERO(), prev_mip.buffer->Size2D()});
		Graphics::DrawQuad();
	}
}

void BlurRenderer::UpdateBuffers(const IntVector2& size, ImageFormat format, const IntVector2& minMipSize, int maxMips)
{
	aspectRatio = static_cast<float>(size.x) / static_cast<float>(size.y);

	IntVector2 min_size {std::max(minMipSize.x, 1), std::max(minMipSize.y, 1)};

	passes.clear();
	for (int i = 0, hw = size.x, hh = size.y; (!maxMips || i < maxMips) && hw >= min_size.x && hh >= min_size.y; ++i, hw /= 2, hh /= 2) {
		MipPass& mip = passes.emplace_back();

		mip.buffer = std::make_unique<Texture>();
		mip.buffer->Define(TARGET_2D, IntVector2 {hw, hh}, format);
		mip.buffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		mip.fbo = std::make_unique<FrameBuffer>();
		mip.fbo->Define(mip.buffer.get(), nullptr);
	}
}

FrameBuffer* BlurRenderer::GetFramebuffer() const
{
	return passes[0].fbo.get();
}

Texture* BlurRenderer::GetTexture() const
{
	return passes[0].buffer.get();
}
