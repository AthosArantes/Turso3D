#pragma once

#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/Vector2.h>
#include <map>
#include <memory>
#include <vector>

namespace Turso3D
{
	class Graphics;
	class Renderer;
	class DebugRenderer;
	class WorkQueue;

	class Scene;
	class Camera;
	class StaticModel;
	class AnimatedModel;
	class SpatialNode;
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

	Turso3D::FrameBuffer mrtFbo[2];
	Turso3D::Texture hdrBuffer[2];
	Turso3D::Texture normalBuffer[2];
	Turso3D::Texture depthStencilBuffer[2];

	Turso3D::FrameBuffer ldrFbo;
	Turso3D::Texture ldrBuffer;

	Turso3D::FrameBuffer bloomFbo;
	Turso3D::Texture bloomBuffer;
	Turso3D::Texture bloomMips[6];
	int maxBloomMip;

	Turso3D::FrameBuffer ssaoFbo;
	Turso3D::Texture ssaoTexture;
	Turso3D::Texture ssaoNoiseTexture;

	std::shared_ptr<Turso3D::Camera> camera;
	std::shared_ptr<Turso3D::Scene> scene;

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
	// Determimnes whether the cursor is inside the client area.
	bool cursorInside;

	float camYaw;
	float camPitch;

	Turso3D::AnimatedModel* characterModel;
	Turso3D::SpatialNode* model;
};
