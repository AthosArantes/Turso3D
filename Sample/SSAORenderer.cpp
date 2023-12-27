#include "SSAORenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/Random.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/IO/Log.h>
#include <glew/glew.h>

namespace Turso3D
{
	SSAORenderer::SSAORenderer(Graphics* graphics) :
		graphics(graphics)
	{
		program = graphics->CreateProgram("PostProcess/SSAO.glsl", "", "");
		uNoiseInvSize = program->Uniform(StringHash {"noiseInvSize"});
		uAoParameters = program->Uniform(StringHash {"aoParameters"});
		uScreenInvSize = program->Uniform(StringHash {"screenInvSize"});
		uFrustumSize = program->Uniform(StringHash {"frustumSize"});
		uDepthReconstruct = program->Uniform(StringHash {"depthReconstruct"});

		blurProgram = graphics->CreateProgram("PostProcess/SSAOBlur.glsl", "", "");
		uBlurInvSize = blurProgram->Uniform(StringHash {"blurInvSize"});

		texBuffer = std::make_unique<Texture>();
		fbo = std::make_unique<FrameBuffer>();

		GenerateNoiseTexture();
	}

	SSAORenderer::~SSAORenderer()
	{
	}

	void SSAORenderer::UpdateBuffers(const IntVector2& size)
	{
		IntVector2 texSize {size.x / 2, size.y / 2};

		if (texBuffer->Width() == texSize.x && texBuffer->Height() == texSize.y) {
			return;
		}

		Log::Scope logScope {"SSAORenderer::UpdateBuffers"};

		texBuffer->Define(TEX_2D, texSize, FMT_RGBA8);
		texBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		fbo->Define(texBuffer.get(), nullptr);

		screenSize = size;
		invScreenSize = Vector2 {1.0f / size.x, 1.0f / size.y};
		invTexSize = Vector2 {1.0f / texSize.x, 1.0f / texSize.y};
	}

	void SSAORenderer::Render(Camera* camera, Texture* normal, Texture* depth, FrameBuffer* dst)
	{
		IntRect screenRect {0, 0, screenSize.x, screenSize.y};
		IntVector2 ssaoSize {texBuffer->Width(), texBuffer->Height()};

		// First sample the normals and depth buffer, then apply a blurred SSAO result that darkens the opaque geometry
		float farClip = camera->FarClip();
		float nearClip = camera->NearClip();
		Vector3 nearVec, farVec;
		camera->FrustumSize(nearVec, farVec);

		// SSAO pass
		program->Bind();
		graphics->SetFrameBuffer(fbo.get());
		graphics->SetViewport(IntRect {0, 0, ssaoSize.x, ssaoSize.y});

		glUniform2f(uNoiseInvSize, ssaoSize.x / 4.0f, ssaoSize.y / 4.0f);
		glUniform4f(uAoParameters, 0.15f, 1.0f, 0.025f, 0.15f);
		glUniform2f(uScreenInvSize, invScreenSize.x, invScreenSize.y);

		glUniform4f(uFrustumSize, farVec.x, farVec.y, farVec.z, (float)screenSize.y / (float)screenSize.x);
		glUniform2f(uDepthReconstruct,
			farClip / (farClip - nearClip),
			-nearClip / (farClip - nearClip)
		);

		depth->Bind(0);
		normal->Bind(1);
		texNoise->Bind(2);
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();

		// Blur pass
		blurProgram->Bind();
		graphics->SetFrameBuffer(dst);
		graphics->SetViewport(screenRect);

		glUniform2f(uBlurInvSize, invTexSize.x, invTexSize.y);
		texBuffer->Bind(0);

		graphics->SetRenderState(BLEND_SUBTRACT, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();
	}

	void SSAORenderer::GenerateNoiseTexture()
	{
		// Random noise texture for SSAO
		unsigned char noiseData[4 * 4 * 4];
		for (int i = 0; i < 4 * 4; ++i) {
			Vector3 noiseVec(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f);
			noiseVec.Normalize();

			noiseData[i * 4 + 0] = (unsigned char)(noiseVec.x * 127.0f + 128.0f);
			noiseData[i * 4 + 1] = (unsigned char)(noiseVec.y * 127.0f + 128.0f);
			noiseData[i * 4 + 2] = (unsigned char)(noiseVec.z * 127.0f + 128.0f);
			noiseData[i * 4 + 3] = 0;
		}
		ImageLevel noiseDataLevel(IntVector2(4, 4), FMT_RGBA8, &noiseData[0]);
		texNoise = std::make_unique<Texture>();
		texNoise->Define(TEX_2D, IntVector2(4, 4), FMT_RGBA8, false, 1, 1, &noiseDataLevel);
		texNoise->DefineSampler(FILTER_POINT);
	}
}
