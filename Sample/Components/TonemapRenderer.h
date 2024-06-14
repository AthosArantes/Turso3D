#pragma once

#include <Turso3D/fwd.h>
#include <memory>

class TonemapRenderer
{
public:
	TonemapRenderer();
	~TonemapRenderer();

	void Initialize(Turso3D::Graphics* graphics);

	// Render tonemap to the currently bound framebuffer.
	void Render(Turso3D::Texture* hdrColor);

private:
	// Cached graphics subsystem.
	Turso3D::Graphics* graphics;

	// Tonemap shader program
	std::shared_ptr<Turso3D::ShaderProgram> program;
	// Exposure shader uniform location
	int uExposure;
};
