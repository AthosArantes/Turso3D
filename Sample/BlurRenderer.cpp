#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/IO/Log.h>

namespace Turso3D
{
	struct BlurRenderer::MipPass
	{
		std::unique_ptr<Texture> buffer;
		std::unique_ptr<FrameBuffer> fbo;
	};

	// ==========================================================================================
	BlurRenderer::BlurRenderer() :
		graphics(nullptr)
	{
	}

	BlurRenderer::~BlurRenderer()
	{
	}

	void BlurRenderer::Initialize(Graphics* graphics_)
	{
		graphics = graphics_;

		constexpr StringHash invSrcSizeHash {"invSrcSize"};
		constexpr StringHash filterRadiusHash {"filterRadius"};
		constexpr StringHash aspectRatioHash {"aspectRatio"};

		constexpr const char* downsampleDefines[] = {"FIRST_PASS", ""};
		for (int i = 0; i < 2; ++i) {
			downsampleProgram[i] = graphics->CreateProgram("PostProcess/BlurDownsample.glsl", downsampleDefines[i], downsampleDefines[i]);
			uInvSrcSize[i] = downsampleProgram[i]->Uniform(invSrcSizeHash);
		}

		upsampleProgram = graphics->CreateProgram("PostProcess/BlurUpsample.glsl", "", "");
		uFilterRadius = upsampleProgram->Uniform(filterRadiusHash);
		uAspectRatio = upsampleProgram->Uniform(aspectRatioHash);
	}

	void BlurRenderer::Downsample(Texture* srcColor)
	{
		for (size_t i = 0; i < passes.size(); ++i) {
			// Bind the program
			int p = int(bool(i));
			downsampleProgram[p]->Bind();

			// Bind the draw buffer
			MipPass& mip = passes[i];
			mip.fbo->Bind();

			// Bind the texture to be sampled
			Texture* src = (i == 0) ? srcColor : passes[i - 1].buffer.get();
			src->Bind(0);

			graphics->SetViewport(IntRect {IntVector2::ZERO(), mip.buffer->Size2D()});
			graphics->SetUniform(uInvSrcSize[p], Vector2 {1.0f / src->Width(), 1.0f / src->Height()});
			graphics->DrawQuad();
		}
	}

	void BlurRenderer::Upsample(float filterRadius)
	{
		upsampleProgram->Bind();
		graphics->SetUniform(uAspectRatio, aspectRatio);
		graphics->SetUniform(uFilterRadius, filterRadius);

		for (size_t i = 0; i < passes.size() - 1; ++i) {
			size_t ri = passes.size() - (1 + i);

			// Bind the draw buffer
			MipPass& prev_mip = passes[ri - 1];
			prev_mip.fbo->Bind();

			// Bind the mip texture to be sampled.
			MipPass& mip = passes[ri];
			mip.buffer->Bind(0);

			graphics->SetViewport(IntRect {IntVector2::ZERO(), prev_mip.buffer->Size2D()});
			graphics->DrawQuad();
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
}
