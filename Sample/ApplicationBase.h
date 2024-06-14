#pragma once

#include <Turso3D/fwd.h>
#include <map>
#include <memory>

// Base class for managing inputs and main game loop
class ApplicationBase
{
	enum class InputState
	{
		Up,
		Released,
		Pressed,
		Down
	};

public:
	ApplicationBase();
	virtual ~ApplicationBase();

	virtual bool Initialize();

	void Run();

protected:
	// Set a software frame limiter.
	// Pass zero (default) to disable.
	void SetFrameLimit(int rate);
	// Set the FixedUpdate frame rate.
	// Pass zero (default) to disable.
	void SetFixedUpdateRate(int rate);

	bool IsKeyDown(int key) const;
	bool IsKeyPressed(int key) const;
	bool IsMouseDown(int button) const;
	bool IsMousePressed(int button) const;
	bool IsMouseInsideWindow() const { return mouseInside; }
	bool IsWindowFocused() const { return windowFocused; }

	virtual void Update(double dt);
	virtual void FixedUpdate(double dt);
	virtual void PostUpdate(double dt);

	virtual void OnFixedUpdateReset();

	virtual void OnKey(int key, int scancode, int action, int mods);
	virtual void OnMouseButton(int button, int action, int mods);
	virtual void OnMouseMove(double xpos, double ypos);
	virtual void OnMouseEnterLeave(bool entered);
	virtual void OnFramebufferSize(int width, int height);
	virtual void OnWindowFocusChanged(int focused);

private:
	void ApplyFrameLimit();
	void UpdateKeys();

protected:
	// Cached graphics subsystem.
	std::unique_ptr<Turso3D::WorkQueue> workQueue;
	std::unique_ptr<Turso3D::Graphics> graphics;
	std::unique_ptr<Turso3D::Renderer> renderer;
	std::unique_ptr<Turso3D::DebugRenderer> debugRenderer;

private:
	double timestamp;
	double deltaTime;
	double deltaTimeAccumulator;

	int frameLimit;
	int fixedRate;

	std::map<int, InputState> keyStates;
	std::map<int, InputState> mouseButtonStates;

	bool windowFocused;
	bool mouseInside;
};
