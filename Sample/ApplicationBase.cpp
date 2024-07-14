#include "ApplicationBase.h"
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Renderer/Renderer.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <GLFW/glfw3.h>
#include <chrono>

using namespace Turso3D;

static inline ApplicationBase* GetAppFromWindow(GLFWwindow* window)
{
	return reinterpret_cast<ApplicationBase*>(glfwGetWindowUserPointer(window));
}

// ==========================================================================================
ApplicationBase::ApplicationBase() :
	timestamp(0),
	deltaTime(0),
	deltaTimeAccumulator(0),
	frameLimit(0),
	fixedRate(0),
	windowFocused(true),
	mouseInside(false)
{
	// Initialize subsystems that don't depend on the application window / OpenGL context
	workQueue = std::make_unique<WorkQueue>();
}

ApplicationBase::~ApplicationBase()
{
	Graphics::ShutDown();
}

bool ApplicationBase::Initialize()
{
	if (Graphics::Initialize("Turso3D renderer test", 1600, 900)) {
		GLFWwindow* window = static_cast<GLFWwindow*>(Graphics::Window());
		glfwSetWindowUserPointer(window, this);

		// Setup glfw callbacks
		glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void
		{
			GetAppFromWindow(window)->OnKey(key, scancode, action, mods);
		});
		glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) -> void
		{
			GetAppFromWindow(window)->OnMouseButton(button, action, mods);
		});
		glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) -> void
		{
			GetAppFromWindow(window)->OnMouseMove(xpos, ypos);
		});
		glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) -> void
		{
			GetAppFromWindow(window)->OnMouseScroll(xoffset, yoffset);
		});
		glfwSetCursorEnterCallback(window, [](GLFWwindow* window, int entered) -> void
		{
			GetAppFromWindow(window)->OnMouseEnterLeave(entered);
		});
		glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) -> void
		{
			GetAppFromWindow(window)->OnFramebufferSize(width, height);
		});
		glfwSetWindowFocusCallback(window, [](GLFWwindow* window, int focused) -> void
		{
			GetAppFromWindow(window)->OnWindowFocusChanged(focused);
		});

		// Initialize renderer/debug renderer.
		renderer = std::make_unique<Renderer>(workQueue.get());
		debugRenderer = std::make_unique<DebugRenderer>();

	} else {
		return false;
	}
	return true;
}

void ApplicationBase::Run()
{
	GLFWwindow* window = static_cast<GLFWwindow*>(Graphics::Window());

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		// Calculate delta time
		double now = glfwGetTime();
		deltaTime = now - timestamp;
		timestamp = now;

		Update(deltaTime);

		// Fixed time update
		if (fixedRate > 0) {
			deltaTimeAccumulator += deltaTime;
			if (deltaTimeAccumulator > 10.0) {
				deltaTimeAccumulator = 0;
				OnFixedUpdateReset();

			} else {
				const double fixed_rate = 1.0 / fixedRate;
				while (deltaTimeAccumulator >= fixed_rate) {
					FixedUpdate(fixed_rate);
					deltaTimeAccumulator -= fixed_rate;
				}
			}
		}

		PostUpdate(deltaTime);
		UpdateKeys();

		glfwPollEvents();
		ApplyFrameLimit();
	}
}

void ApplicationBase::ApplyFrameLimit()
{
	if (frameLimit <= 0) {
		return;
	}

	// Skip frame limiter if in fullscreen mode, vsync is on and the refresh rate matches the frame limit value.
	if (Graphics::VSync() && Graphics::FullscreenRefreshRate() == frameLimit) {
		return;
	}

	const double target_rate = 1.0 / static_cast<double>(frameLimit);
	while (true) {
		double rate = glfwGetTime() - timestamp;
		if (rate < target_rate) {
			double d = (rate - target_rate);
			if (d > 0.001) {
				std::this_thread::yield();
				//std::this_thread::sleep_for(std::chrono::duration<double> {d - 0.001});
			}
			// spin-lock on less than 1ms of desired frame rate
		} else {
			break;
		}
	}
}

void ApplicationBase::UpdateKeys()
{
	// Update key states
	for (auto it = keyStates.begin(); it != keyStates.end(); ++it) {
		if (it->second == InputState::Released) {
			it->second = InputState::Up;
		} else if (it->second == InputState::Pressed) {
			it->second = InputState::Down;
		}
	}

	// Update mouse button states
	for (auto it = mouseButtonStates.begin(); it != mouseButtonStates.end(); ++it) {
		if (it->second == InputState::Released) {
			it->second = InputState::Up;
		} else if (it->second == InputState::Pressed) {
			it->second = InputState::Down;
		}
	}
}

void ApplicationBase::SetFrameLimit(int rate)
{
	frameLimit = rate;
}

void ApplicationBase::SetFixedUpdateRate(int rate)
{
	fixedRate = rate;
}

bool ApplicationBase::IsKeyDown(int key) const
{
	auto it = keyStates.find(key);
	if (it != keyStates.end()) {
		return (it->second == InputState::Pressed) || (it->second == InputState::Down);
	}
	return false;
}

bool ApplicationBase::IsKeyPressed(int key) const
{
	auto it = keyStates.find(key);
	if (it != keyStates.end()) {
		return (it->second == InputState::Pressed);
	}
	return false;
}

bool ApplicationBase::IsMouseDown(int button) const
{
	auto it = mouseButtonStates.find(button);
	if (it != mouseButtonStates.end()) {
		return (it->second == InputState::Pressed) || (it->second == InputState::Down);
	}
	return false;
}

bool ApplicationBase::IsMousePressed(int button) const
{
	auto it = mouseButtonStates.find(button);
	if (it != mouseButtonStates.end()) {
		return (it->second == InputState::Pressed);
	}
	return false;
}

// ==========================================================================================
void ApplicationBase::Update(double dt)
{
}

void ApplicationBase::FixedUpdate(double dt)
{
}

void ApplicationBase::PostUpdate(double dt)
{
}

void ApplicationBase::OnFixedUpdateReset()
{
}

// ==========================================================================================
void ApplicationBase::OnKey(int key, int scancode, int action, int mods)
{
	if (!windowFocused) {
		return;
	}
	if (action == GLFW_PRESS) {
		keyStates[key] = InputState::Pressed;
	} else if (action == GLFW_RELEASE) {
		keyStates[key] = InputState::Released;
	}
}

void ApplicationBase::OnMouseButton(int button, int action, int mods)
{
	if (!mouseInside || !windowFocused) {
		return;
	}
	if (action == GLFW_PRESS) {
		mouseButtonStates[button] = InputState::Pressed;
	} else if (action == GLFW_RELEASE) {
		mouseButtonStates[button] = InputState::Released;
	}
}

void ApplicationBase::OnMouseMove(double xpos, double ypos)
{
}

void ApplicationBase::OnMouseScroll(double xoffset, double yoffset)
{
}

void ApplicationBase::OnMouseEnterLeave(bool entered)
{
	mouseInside = entered;
	if (!mouseInside) {
		// Release all mouse button when leaving
		for (auto& it : mouseButtonStates) {
			InputState& state = it.second;
			if (state == InputState::Pressed || state == InputState::Down) {
				OnMouseButton(it.first, GLFW_RELEASE, 0);
			}
		}
	}
}

void ApplicationBase::OnFramebufferSize(int width, int height)
{
}

void ApplicationBase::OnWindowFocusChanged(int focused)
{
	windowFocused = focused;
}
