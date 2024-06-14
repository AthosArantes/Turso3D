#include "TonemapRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

TonemapRenderer::TonemapRenderer() :
	graphics(nullptr),
	uExposure(0)
{
}

TonemapRenderer::~TonemapRenderer()
{
}

void TonemapRenderer::Initialize(Graphics* graphics_)
{
	graphics = graphics_;

	constexpr StringHash exposureHash {"exposure"};

	program = graphics->CreateProgram("PostProcess/Tonemap.glsl", "", "");
	uExposure = program->Uniform(exposureHash);
}

void TonemapRenderer::Render(Texture* hdrColor)
{
	program->Bind();

	// TODO: exposure based on eye-adaptation formula
	graphics->SetUniform(uExposure, 0.1f);
	hdrColor->Bind(0);

	graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
	graphics->DrawQuad();
}
