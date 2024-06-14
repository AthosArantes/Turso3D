#include "SSAORenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Math/Random.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

SSAORenderer::SSAORenderer() :
	graphics(nullptr)
{
	ssaoUniformBuffer = std::make_unique<UniformBuffer>();
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

	constexpr StringHash blurInvSizeHash {"blurInvSize"};
	ssaoProgram = graphics->CreateProgram("PostProcess/SSAO.glsl", "", "");

	blurProgram = graphics->CreateProgram("PostProcess/SSAOBlur.glsl", "", "");
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
		uniformData.noiseInvSize = Vector2 {(float)resultTexture->Width() / noiseTexture->Width(), (float)resultTexture->Height() / noiseTexture->Height()};
		uniformData.screenInvSize = Vector2 {1.0f / viewRect.Width(), 1.0f / viewRect.Height()};

		ssaoUniformBuffer->SetData(0, sizeof(UniformDataBlock), &uniformData, false);
		uniformDataDirty = false;
	}

	// SSAO pass
	resultFbo->Bind();
	ssaoProgram->Bind();
	graphics->SetViewport(IntRect {IntVector2::ZERO(), resultTexture->Size2D()});

	depth->Bind(0);
	normal->Bind(1);
	noiseTexture->Bind(2);

	ssaoUniformBuffer->Bind(UB_CUSTOM);

	graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
	graphics->DrawQuad();

	//UniformBuffer::Unbind(UB_CUSTOM);

	// Blur pass
	dst->Bind();
	blurProgram->Bind();
	graphics->SetViewport(viewRect);

	resultTexture->Bind(0);
	graphics->SetUniform(uBlurInvSize, Vector2 {1.0f / resultTexture->Width(), 1.0f / resultTexture->Height()});

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

	uniformDataDirty = true;
}
