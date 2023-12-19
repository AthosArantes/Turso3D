#include "BloomRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

#define MIN_MIP_SIZE 8

namespace Turso3D
{
	BloomRenderer::BloomRenderer(Graphics* graphics) :
		graphics(graphics),
		brightThreshold(3.0f),
		filterRadius(0.005f),
		intensity(0.05f)
	{
		brightProgram = graphics->CreateProgram("PostProcess/Brightness.glsl", "", "");
		uBrightThreshold = brightProgram->Uniform(StringHash {"threshold"});

		downsampleProgram = graphics->CreateProgram("PostProcess/Downsample.glsl", "", "");
		uInvTexSize = downsampleProgram->Uniform(StringHash {"invSrcSize"});

		upsampleProgram = graphics->CreateProgram("PostProcess/Upsample.glsl", "", "");
		uFilterRadius = upsampleProgram->Uniform(StringHash {"filterRadius"});

		composeProgram = graphics->CreateProgram("PostProcess/BloomCompose.glsl", "", "");
		uIntensity = composeProgram->Uniform(StringHash {"intensity"});

		texBuffer = std::make_unique<Texture>();
		fbo = std::make_unique<FrameBuffer>();
	}

	BloomRenderer::~BloomRenderer()
	{
	}

	void BloomRenderer::SetParameters(float brightThreshold_, float filterRadius_, float intensity_)
	{
		brightThreshold = brightThreshold_;
		filterRadius = filterRadius_;
		intensity = intensity_;
	}

	void BloomRenderer::UpdateBuffers(const IntVector2& size)
	{
		if (texBuffer->Width() == size.x && texBuffer->Height() == size.y) {
			return;
		}

		Log::Scope logScope {"BloomRenderer"};

		texBuffer->Define(TEX_2D, size, FMT_R11_G11_B10F);
		texBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		fbo->Define(texBuffer.get(), nullptr);

		// (Re)create mip passes
		mipPasses.clear();
		for (int i = 0, hw = size.x / 2, hh = size.y / 2; hw >= MIN_MIP_SIZE && hh >= MIN_MIP_SIZE; ++i, hw /= 2, hh /= 2) {
			MipPass& mip = mipPasses.emplace_back();

			mip.buffer = std::make_unique<Texture>();
			mip.buffer->Define(TEX_2D, IntVector2(hw, hh), FMT_R11_G11_B10F);
			mip.buffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

			mip.downsampleFbo = std::make_unique<FrameBuffer>();
			mip.downsampleFbo->Define(mip.buffer.get(), nullptr);

			// The upsample draws on previous (larger) mip texture
			mip.upsampleFbo = std::make_unique<FrameBuffer>();
			mip.upsampleFbo->Define((i == 0) ? (Texture*)nullptr : mipPasses[i - 1].buffer.get(), nullptr);
		}
	}

	void BloomRenderer::Render(Texture* hdrColor)
	{
		graphics->SetFrameBuffer(fbo.get());
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->SetViewport(IntRect(0, 0, texBuffer->Width(), texBuffer->Height()));

		// Sample bright areas
		brightProgram->Bind();
		glUniform1f(uBrightThreshold, brightThreshold);
		hdrColor->Bind(0);
		graphics->DrawQuad();

		size_t mipCount = mipPasses.size();

		// Downsample
		downsampleProgram->Bind();
		for (size_t i = 0; i < mipCount; ++i) {
			MipPass& mip = mipPasses[i];

			graphics->SetFrameBuffer(mip.downsampleFbo.get());
			graphics->SetViewport(IntRect(0, 0, mip.buffer->Width(), mip.buffer->Height()));

			Texture* src = (i == 0) ? texBuffer.get() : mipPasses[i - 1].buffer.get();
			src->Bind(0);
			glUniform2f(uInvTexSize, 1.0f / src->Width(), 1.0f / src->Height());

			graphics->DrawQuad();
		}

		// Upsample
		upsampleProgram->Bind();
		glUniform1f(uFilterRadius, filterRadius);
		for (size_t i = 0; i < mipCount; ++i) {
			size_t index = mipCount - i - 1;
			if (index == 0) {
				// Skip mip[0] upsample.
				break;
			}

			MipPass& mip = mipPasses[index];

			graphics->SetFrameBuffer(mip.upsampleFbo.get());

			Texture* src = mipPasses[index - 1].buffer.get();
			graphics->SetViewport(IntRect(0, 0, src->Width(), src->Height()));

			mip.buffer->Bind(0);
			graphics->DrawQuad();
		}

		// Compose
		composeProgram->Bind();
		glUniform1f(uIntensity, intensity);
		graphics->SetFrameBuffer(fbo.get());
		graphics->SetViewport(IntRect(0, 0, texBuffer->Width(), texBuffer->Height()));
		hdrColor->Bind(0);
		mipPasses[0].buffer->Bind(1);
		graphics->DrawQuad();
	}
}
