#include "BlurRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>
#include <cassert>

namespace Turso3D
{
	constexpr size_t MIN_MIP_SIZE = 4;

	// ==========================================================================================
	BlurRenderer::BlurRenderer()
	{
		resultTexture = std::make_unique<Texture>();
		resultFbo = std::make_unique<FrameBuffer>();
	}

	BlurRenderer::~BlurRenderer()
	{
	}

	void BlurRenderer::Initialize(Graphics* graphics_)
	{
		graphics = graphics_;

		programDownsample = graphics->CreateProgram("PostProcess/BlurDownsample.glsl", "", "");
		uInvSrcSize = programDownsample->Uniform(StringHash {"invSrcSize"});

		programUpsample = graphics->CreateProgram("PostProcess/BlurUpsample.glsl", "", "");
		uFilterRadius = programUpsample->Uniform(StringHash {"filterRadius"});
	}

	void BlurRenderer::UpdateBuffers(const IntVector2& size, ImageFormat format, size_t maxMips)
	{
		Log::Scope logScope {"BlurRenderer::UpdateBuffers"};

		resultTexture->Define(TEX_2D, size, format);
		resultTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		resultFbo->Define(resultTexture.get(), nullptr);

		// (Re)create mip passes
		mipPasses.clear();
		for (int i = 0, hw = size.x / 2, hh = size.y / 2; hw >= MIN_MIP_SIZE && hh >= MIN_MIP_SIZE; ++i, hw /= 2, hh /= 2) {
			MipPass& mip = mipPasses.emplace_back();

			mip.texture = std::make_unique<Texture>();
			mip.texture->Define(TEX_2D, IntVector2 {hw, hh}, format);
			mip.texture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

			mip.fbo = std::make_unique<FrameBuffer>();
			mip.fbo->Define(mip.texture.get(), nullptr);

			if (maxMips && i >= maxMips - 1) {
				break;
			}
		}
	}

	void BlurRenderer::Render(Texture* color, float filterRadius)
	{
		size_t mipCount = mipPasses.size();
		assert(mipCount);

		// Downsample
		programDownsample->Bind();
		for (size_t i = 0; i < mipCount; ++i) {
			MipPass& mip = mipPasses[i];

			graphics->SetFrameBuffer(mip.fbo.get());
			graphics->SetViewport(IntRect(0, 0, mip.texture->Width(), mip.texture->Height()));

			Texture* src = (i == 0) ? color : mipPasses[i - 1].texture.get();
			src->Bind(0);
			glUniform2f(uInvSrcSize, 1.0f / src->Width(), 1.0f / src->Height());

			graphics->DrawQuad();
		}

		// Upsample
		programUpsample->Bind();
		glUniform1f(uFilterRadius, filterRadius);
		for (size_t i = mipCount - 1; i > 0; --i) {
			MipPass& mip = mipPasses[i];
			MipPass& prevMip = mipPasses[i - 1];

			graphics->SetFrameBuffer(prevMip.fbo.get());
			graphics->SetViewport(IntRect(0, 0, prevMip.texture->Width(), prevMip.texture->Height()));
			mip.texture->Bind(0);
			graphics->DrawQuad();
		}

		// Blit to the result framebuffer
		IntRect rcDst {0, 0, resultTexture->Width(), resultTexture->Height()};
		IntRect rcSrc {0, 0, mipPasses[0].texture->Width(), mipPasses[0].texture->Height()};
		graphics->Blit(resultFbo.get(), rcDst, mipPasses[0].fbo.get(), rcSrc, true, false, FILTER_BILINEAR);
	}
}
