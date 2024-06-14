#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/fwd.h>
#include <memory>

class BlurRenderer;

class BloomRenderer
{
public:
	BloomRenderer();
	~BloomRenderer();

	void Initialize(Turso3D::Graphics* graphics);

	void UpdateBuffers(const Turso3D::IntVector2& size, Turso3D::ImageFormat format);
	void Render(Turso3D::Texture* hdrColor, float intensity = 0.05f);

	Turso3D::FrameBuffer* GetFramebuffer() const { return fbo.get(); }
	Turso3D::Texture* GetTexture() const { return buffer.get(); }

private:
	// Cached graphics subsystem.
	Turso3D::Graphics* graphics;

	std::unique_ptr<BlurRenderer> blurRenderer;

	std::shared_ptr<Turso3D::ShaderProgram> bloomProgram;
	int uIntensity; // intensity uniform location.

	std::unique_ptr<Turso3D::Texture> buffer;
	std::unique_ptr<Turso3D::FrameBuffer> fbo;
};
