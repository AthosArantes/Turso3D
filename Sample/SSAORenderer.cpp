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
	SSAORenderer::SSAORenderer()
	{
		noiseTexture = std::make_unique<Texture>();
		resultTexture = std::make_unique<Texture>();
		resultFbo = std::make_unique<FrameBuffer>();
	}

	SSAORenderer::~SSAORenderer()
	{
	}

	void SSAORenderer::Initialize(Graphics* graphics_)
	{
		graphics = graphics_;

		program = graphics->CreateProgram("PostProcess/SSAO.glsl", "", "");
		uNoiseInvSize = program->Uniform(StringHash {"noiseInvSize"});
		uAoParameters = program->Uniform(StringHash {"aoParameters"});
		uScreenInvSize = program->Uniform(StringHash {"screenInvSize"});
		uFrustumSize = program->Uniform(StringHash {"frustumSize"});
		uDepthReconstruct = program->Uniform(StringHash {"depthReconstruct"});

		programBlur = graphics->CreateProgram("PostProcess/SSAOBlur.glsl", "", "");
		uBlurInvSize = programBlur->Uniform(StringHash {"blurInvSize"});

		GenerateNoiseTexture();
	}

	void SSAORenderer::UpdateBuffers(const IntVector2& size)
	{
		Log::Scope logScope {"SSAORenderer::UpdateBuffers"};
		IntVector2 texSize {size.x / 2, size.y / 2};

		resultTexture->Define(TARGET_2D, texSize, FORMAT_RGBA8_UNORM_PACK32);
		resultTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		resultFbo->Define(resultTexture.get(), nullptr);
	}

	void SSAORenderer::Render(Camera* camera, Texture* normal, Texture* depth, FrameBuffer* dst, const IntRect& viewRect)
	{
		//IntRect screenRect {0, 0, screenSize.x, screenSize.y};
		IntVector2 ssaoSize {resultTexture->Width(), resultTexture->Height()};

		// First sample the normals and depth buffer, then apply a blurred SSAO result that darkens the opaque geometry
		float farClip = camera->FarClip();
		float nearClip = camera->NearClip();
		Vector3 nearVec, farVec;
		camera->FrustumSize(nearVec, farVec);

		// SSAO pass
		program->Bind();
		graphics->SetFrameBuffer(resultFbo.get());
		graphics->SetViewport(IntRect {0, 0, ssaoSize.x, ssaoSize.y});

		depth->Bind(0);
		normal->Bind(1);
		noiseTexture->Bind(2);

		glUniform2f(uNoiseInvSize, ssaoSize.x / 4.0f, ssaoSize.y / 4.0f);
		glUniform4f(uAoParameters, 0.15f, 1.0f, 0.025f, 0.15f);
		glUniform2f(uScreenInvSize, 1.0f / viewRect.Width(), 1.0f / viewRect.Height());

		glUniform4f(uFrustumSize, farVec.x, farVec.y, farVec.z, (float)viewRect.Height() / (float)viewRect.Width());
		glUniform2f(uDepthReconstruct,
			farClip / (farClip - nearClip),
			-nearClip / (farClip - nearClip)
		);

		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();

		// Blur pass
		programBlur->Bind();
		graphics->SetFrameBuffer(dst);
		graphics->SetViewport(viewRect);

		resultTexture->Bind(0);
		glUniform2f(uBlurInvSize, 1.0f / ssaoSize.x, 1.0f / ssaoSize.y);
		
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
		ImageLevel noiseDataLevel {&noiseData[0], 0, IntBox {0, 0, 0, 4, 4, 0}, 0, 0};

		noiseTexture->Define(TARGET_2D, IntVector3 {4, 4, 1}, FORMAT_RGBA8_UNORM_PACK32, 1, 1);
		noiseTexture->DefineSampler(FILTER_POINT);
		noiseTexture->SetData(noiseDataLevel);
	}
}
