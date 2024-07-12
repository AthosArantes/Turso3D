#include "TonemapRenderer.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>

using namespace Turso3D;

TonemapRenderer::TonemapRenderer() :
	uExposure(0)
{
}

TonemapRenderer::~TonemapRenderer()
{
}

void TonemapRenderer::Initialize()
{
	constexpr StringHash exposureHash {"exposure"};

	program = Graphics::CreateProgram("PostProcess/Tonemap.glsl", "", "");
	uExposure = program->Uniform(exposureHash);
}

void TonemapRenderer::Render(Texture* hdrColor)
{
	Graphics::BindProgram(program.get());

	// TODO: exposure based on eye-adaptation formula
	Graphics::SetUniform(uExposure, 0.1f);
	Graphics::BindTexture(0, hdrColor);

	Graphics::DrawQuad();
}
