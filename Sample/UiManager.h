#pragma once

#include <Turso3D/fwd.h>
#include <memory>

class UiManager
{
	struct Middleware;

public:
	UiManager();
	~UiManager();

	void Initialize();
	void UpdateBuffers(const Turso3D::IntVector2& size);

	// Update/Render ui system.
	void Update(double dt);

	// Compose the rendered ui to the currently bound framebuffer.
	void Compose(Turso3D::Texture* background, Turso3D::Texture* blurredBackground);

private:
	std::shared_ptr<Turso3D::ShaderProgram> composeProgram;

	std::unique_ptr<Middleware> impl;
};
