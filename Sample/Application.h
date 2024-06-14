#pragma once

#include "ApplicationBase.h"
#include <Turso3D/Math/Vector2.h>
#include <Turso3D/fwd.h>
#include <memory>

class BlurRenderer;
class BloomRenderer;
class SSAORenderer;
class TonemapRenderer;
class UiManager;

class Application : public ApplicationBase
{
public:
	Application();
	~Application();

	bool Initialize() override;

protected:
	void OnMouseMove(double xpos, double ypos) override;
	void OnFramebufferSize(int width, int height) override;

	void CreateTextures();
	void SetupEnvironmentLighting();
	void CreateSpheresScene();
	void CreateThousandMushroomScene();
	void CreateWalkingCharacter();
	void CreateHugeWalls();

	void Update(double dt) override;
	void PostUpdate(double dt) override;
	void FixedUpdate(double dt) override;

	void Render(double dt);

private:
	std::shared_ptr<Turso3D::Camera> camera;
	std::shared_ptr<Turso3D::Scene> scene;

	// Color/Normal buffers
	std::unique_ptr<Turso3D::Texture> colorBuffer;
	std::unique_ptr<Turso3D::Texture> normalBuffer;
	std::unique_ptr<Turso3D::Texture> depthBuffer;
	std::unique_ptr<Turso3D::RenderBuffer> colorRbo;
	std::unique_ptr<Turso3D::RenderBuffer> normalRbo;
	std::unique_ptr<Turso3D::RenderBuffer> depthRbo;
	std::unique_ptr<Turso3D::FrameBuffer> hdrFbo;

	std::unique_ptr<Turso3D::FrameBuffer> colorFbo[2]; // Framebuffers for resolving multisampled colorRbo
	std::unique_ptr<Turso3D::FrameBuffer> normalFbo[2]; // Framebuffers for resolving multisampled normalRbo
	std::unique_ptr<Turso3D::FrameBuffer> depthFbo[2]; // Framebuffers for resolving multisampled depthRbo

	std::unique_ptr<Turso3D::Texture> ldrBuffer;
	std::unique_ptr<Turso3D::FrameBuffer> ldrFbo;

	std::unique_ptr<BlurRenderer> blurRenderer;
	std::unique_ptr<BloomRenderer> bloomRenderer;
	std::unique_ptr<SSAORenderer> aoRenderer;
	std::unique_ptr<TonemapRenderer> tonemapRenderer;

	std::unique_ptr<UiManager> uiManager;

	// Framebuffer multisample level.
	int multiSample;

	// The cursor position (in pixels).
	Turso3D::Vector2 cursorPos;
	// The cursor speed (in pixels) calculated from current cursor position and previous one.
	Turso3D::Vector2 cursorSpeed;
	// Current camera rotation.
	// Used for look around.
	Turso3D::Vector2 camRotation;

	bool useOcclusion;
	bool renderDebug;

	Turso3D::AnimatedModel* character;
};
