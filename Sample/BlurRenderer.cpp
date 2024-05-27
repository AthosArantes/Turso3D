#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/IO/Log.h>

namespace Turso3D
{
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

		downsampleProgram = graphics->CreateProgram("PostProcess/BlurDownsample.glsl", "", "");
		uInvSrcSize = downsampleProgram->Uniform(invSrcSizeHash);

		upsampleProgram = graphics->CreateProgram("PostProcess/BlurUpsample.glsl", "", "");
		uFilterRadius = upsampleProgram->Uniform(filterRadiusHash);
	}

	void BlurRenderer::Render(std::vector<MipPass>& passes, Texture* srcColor, float filterRadius)
	{
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);

		// Downsample
		downsampleProgram->Bind();
		for (size_t i = 0; i < passes.size(); ++i) {
			MipPass& mip = passes[i];

			mip.fbo->Bind();
			graphics->SetViewport(IntRect {IntVector2::ZERO(), mip.buffer->Size2D()});

			Texture* src = (i == 0) ? srcColor : passes[i - 1].buffer.get();
			src->Bind(0);

			graphics->SetUniform(uInvSrcSize, Vector2(1.0f / src->Width(), 1.0f / src->Height()));
			graphics->DrawQuad();
		}

		// Upsample
		upsampleProgram->Bind();
		graphics->SetUniform(uFilterRadius, filterRadius);
		for (size_t i = 0; i < passes.size() - 1; ++i) {
			size_t ri = passes.size() - (1 + i);

			MipPass& mip = passes[ri];
			MipPass& prevMip = passes[ri - 1];

			prevMip.fbo->Bind();
			graphics->SetViewport(IntRect {IntVector2::ZERO(), prevMip.buffer->Size2D()});

			mip.buffer->Bind(0);

			graphics->DrawQuad();
		}
	}

	// ==========================================================================================
	void BlurRenderer::CreateMips(std::vector<MipPass>& passes, const IntVector2& size, ImageFormat format, const IntVector2& minMipSize, size_t maxMips)
	{
		IntVector2 min_size {std::max(minMipSize.x, 1), std::max(minMipSize.y, 1)};

		for (int i = 0, hw = size.x, hh = size.y; (!maxMips || i < maxMips) && hw >= min_size.x && hh >= min_size.y; ++i, hw /= 2, hh /= 2) {
			MipPass& mip = passes.emplace_back();

			mip.buffer = std::make_unique<Texture>();
			mip.buffer->Define(TARGET_2D, IntVector2 {hw, hh}, format);
			mip.buffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

			mip.fbo = std::make_unique<FrameBuffer>();
			mip.fbo->Define(mip.buffer.get(), nullptr);
		}
	}
}
