#include "SSAORenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

SSAORenderer::SSAORenderer()
{
	ssaoUniformBuffer = std::make_unique<UniformBuffer>();
	noiseTexture = std::make_unique<Texture>();
	resultTexture = std::make_unique<Texture>();
	resultFbo = std::make_unique<FrameBuffer>();
}

SSAORenderer::~SSAORenderer()
{
}

void SSAORenderer::Initialize()
{
	constexpr StringHash blurInvSizeHash {"blurInvSize"};
	ssaoProgram = Graphics::CreateProgram("post_process/ssao.glsl", "", "");

	blurProgram = Graphics::CreateProgram("post_process/ssao_blur.glsl", "", "");
	uBlurInvSize = blurProgram->Uniform(blurInvSizeHash);

	ssaoUniformBuffer->Define(USAGE_DEFAULT, sizeof(UniformDataBlock), &uniformData);

	// Default AO parameters
	uniformData.aoParams = Vector4 {0.15f, 1.0f, 0.025f, 0.15f};

	GenerateNoiseTexture();
}

void SSAORenderer::UpdateBuffers(const IntVector2& size)
{
	Log::Scope logScope {"SSAORenderer::UpdateBuffers"};
	IntVector2 texSize {size.x / 2, size.y / 2};

	resultTexture->Define(TARGET_2D, texSize, FORMAT_RGB8_UNORM_PACK8);
	resultTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	resultFbo->Define(resultTexture.get(), nullptr);

	uniformDataDirty = true;
}

void SSAORenderer::Render(Camera* camera, Texture* normal, Texture* depth, FrameBuffer* dst, const IntRect& viewRect)
{
	const IntVector2& result_sz = resultTexture->Size2D();

	// Update uniforms
	Vector3 near, far;
	camera->FrustumSize(near, far);
	Vector4 frustumSize {far.x, far.y, far.z, (float)viewRect.Height() / (float)viewRect.Width()};
	if (frustumSize != uniformData.frustumSize) {
		uniformData.frustumSize = frustumSize;
		uniformDataDirty = true;
	}

	Vector2 clipParams {camera->NearClip(), camera->FarClip()};
	Vector2 depthReconstruct {
		clipParams.y / (clipParams.y - clipParams.x),
		-clipParams.x / (clipParams.y - clipParams.x)
	};
	if (depthReconstruct != uniformData.depthReconstruct) {
		uniformData.depthReconstruct = depthReconstruct;
		uniformDataDirty = true;
	}

	if (uniformDataDirty) {
		const IntVector2& noise_sz = noiseTexture->Size2D();

		uniformData.noiseInvSize = Vector2 {
			static_cast<float>(result_sz.x) / static_cast<float>(noise_sz.x),
			static_cast<float>(result_sz.y) / static_cast<float>(noise_sz.y)
		};
		uniformData.screenInvSize = Vector2 {1.0f / viewRect.Width(), 1.0f / viewRect.Height()};

		ssaoUniformBuffer->SetData(0, sizeof(UniformDataBlock), &uniformData, false);
		uniformDataDirty = false;
	}

	// SSAO pass
	Graphics::BindFramebuffer(resultFbo.get(), nullptr);
	Graphics::BindProgram(ssaoProgram.get());
	Graphics::SetViewport(IntRect {IntVector2::ZERO(), resultTexture->Size2D()});

	Graphics::BindTexture(0, depth);
	Graphics::BindTexture(1, normal);
	Graphics::BindTexture(2, noiseTexture.get());

	Graphics::BindUniformBuffer(UB_CUSTOM, ssaoUniformBuffer.get());

	Graphics::SetRenderState(BLEND_REPLACE, CULL_BACK, CMP_ALWAYS, true, false);
	Graphics::DrawQuad();

	// Blur pass
	Graphics::BindFramebuffer(dst, nullptr);
	Graphics::BindProgram(blurProgram.get());
	Graphics::SetViewport(viewRect);

	Graphics::BindTexture(0, resultTexture.get());
	Graphics::SetUniform(uBlurInvSize, Vector2 {1.0f / static_cast<float>(result_sz.x), 1.0f / static_cast<float>(result_sz.y)});

	Graphics::SetRenderState(BLEND_SUBTRACT, CULL_BACK, CMP_ALWAYS, true, false);
	Graphics::DrawQuad();
}

void SSAORenderer::GenerateNoiseTexture()
{
	// Random noise texture for SSAO
	uint8_t noiseData[4 * 4 * 4];
	for (int i = 0; i < 4 * 4; ++i) {
		Vector3 random {
			static_cast<float>(std::rand()) / RAND_MAX,
			static_cast<float>(std::rand()) / RAND_MAX,
			static_cast<float>(std::rand()) / RAND_MAX
		};
		Vector3 noise = random * 2.0f - Vector3::ONE();
		noise.Normalize();

		noiseData[i * 4 + 0] = (uint8_t)(noise.x * 127.0f + 128.0f);
		noiseData[i * 4 + 1] = (uint8_t)(noise.y * 127.0f + 128.0f);
		noiseData[i * 4 + 2] = (uint8_t)(noise.z * 127.0f + 128.0f);
		noiseData[i * 4 + 3] = 0;
	}
	ImageLevel noiseDataLevel {&noiseData[0], 0, IntBox {0, 0, 0, 4, 4, 0}, 0, 0};

	noiseTexture->Define(TARGET_2D, IntVector3 {4, 4, 1}, FORMAT_RGBA8_UNORM_PACK32, 1, 1);
	noiseTexture->DefineSampler(FILTER_POINT);
	noiseTexture->SetData(noiseDataLevel);

	uniformDataDirty = true;
}
