#pragma once

#include <Turso3D/Math/Vector2.h>
#include <map>
#include <memory>
#include <vector>

namespace Rml
{
	class Context;
}

namespace Turso3D
{
	class DebugRenderer;
	class FrameBuffer;
	class Graphics;
	class Renderer;
	class ShaderProgram;
	class Texture;
	class WorkQueue;

	class AnimatedModel;
	class Camera;
	class Scene;
	class SpatialNode;
	class StaticModel;

	class BloomRenderer;
	class SSAORenderer;

	class RmlFile;
	class RmlSystem;
	class RmlRenderer;
}

class Application
{
	enum class InputState
	{
		Up,
		Released,
		Pressed,
		Down
	};

public:
	Application();
	~Application();

	bool Initialize();
	void Run();

private:
	void ApplyFrameLimit();

	void CreateTextures();
	void CreateDefaultScene();

	bool IsKeyDown(int key);
	bool IsKeyPressed(int key);
	bool IsMouseDown(int button);

	void OnKey(int key, int scancode, int action, int mods);
	void OnMouseButton(int button, int action, int mods);
	void OnMouseMove(double xpos, double ypos);
	void OnMouseEnterLeave(bool entered);
	void OnFramebufferSize(int width, int height);
	void OnWindowFocusChanged(int focused);

	void Update(double dt);
	void PostUpdate(double dt);
	void FixedUpdate(double dt);
	void Render(double dt);

private:
	// Cached graphics subsystem.
	std::unique_ptr<Turso3D::WorkQueue> workQueue;
	std::unique_ptr<Turso3D::Graphics> graphics;
	std::unique_ptr<Turso3D::Renderer> renderer;
	std::unique_ptr<Turso3D::DebugRenderer> debugRenderer;

	std::unique_ptr<Turso3D::FrameBuffer> mrtFbo[2];
	std::unique_ptr<Turso3D::Texture> hdrBuffer[2];
	std::unique_ptr<Turso3D::Texture> normalBuffer[2];
	std::unique_ptr<Turso3D::Texture> depthStencilBuffer[2];

	std::unique_ptr<Turso3D::FrameBuffer> ldrFbo;
	std::unique_ptr<Turso3D::Texture> ldrBuffer;

	// Tonemap shader program
	std::shared_ptr<Turso3D::ShaderProgram> tonemapProgram;
	// Tonemap exposure shader uniform
	int uTonemapExposure;

	std::unique_ptr<Turso3D::BloomRenderer> bloomRenderer;
	std::unique_ptr<Turso3D::SSAORenderer> ssaoRenderer;

	std::shared_ptr<Turso3D::Camera> camera;
	std::shared_ptr<Turso3D::Scene> scene;

	std::unique_ptr<Turso3D::RmlFile> rmlFile;
	std::unique_ptr<Turso3D::RmlSystem> rmlSystem;
	std::unique_ptr<Turso3D::RmlRenderer> rmlRenderer;
	Rml::Context* mainContext;

	int multiSample;

	double timestamp;
	double deltaTime;
	double deltaTimeAccumulator;

	int frameLimit;

	std::map<int, InputState> keyStates;
	std::map<int, InputState> mouseButtonStates;

	Turso3D::Vector2 prevCursorPos;
	// The cursor speed (in pixels) calculated from current cursor position and previous one.
	Turso3D::Vector2 cursorSpeed;
	// Determines whether the cursor is inside the client area.
	bool cursorInside;
	// Variable used to delay mouse movement capturing.
	// Prevents sudden changes that happens in the same frame the mouse changed mode.
	bool captureMouse;

	float camYaw;
	float camPitch;

	Turso3D::AnimatedModel* characterModel;
	Turso3D::SpatialNode* model;
};
