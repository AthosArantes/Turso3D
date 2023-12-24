#include "Application.h"
#include "BloomRenderer.h"
#include "ModelConverter.h"
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Math.h>
#include <Turso3D/Math/Random.h>
#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Renderer/Animation.h>
#include <Turso3D/Renderer/AnimationState.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Light.h>
#include <Turso3D/Renderer/LightEnvironment.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/Renderer.h>
#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Scene/Scene.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <chrono>

using namespace Turso3D;

constexpr int DIRECTIONAL_LIGHT_SIZE = 8192 / 2;
constexpr int LIGHT_ATLAS_SIZE = 8192 / 2;

inline Application* GetAppFromWindow(GLFWwindow* window)
{
	return reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
}

// ==========================================================================================
Application::Application() :
	multiSample(1),
	timestamp(0),
	deltaTime(0),
	deltaTimeAccumulator(0),
	frameLimit(60),
	cursorInside(false),
	camYaw(0),
	camPitch(0),
	characterModel(nullptr),
	model(nullptr)
{
	// Create subsystems that don't depend on the application window / OpenGL context
	workQueue = std::make_unique<WorkQueue>();
	workQueue->CreateWorkerThreads(2);

	ResourceCache* cache = ResourceCache::Instance();
	cache->AddResourceDir((std::filesystem::current_path() / "Shaders").string());
	cache->AddResourceDir((std::filesystem::current_path() / "Data").string());

	// Initialize graphics
	graphics = std::make_unique<Graphics>();

	if (graphics->Initialize("Turso3D renderer test", IntVector2(1600, 900))) {
		GLFWwindow* window = graphics->Window();
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

	} else {
		graphics.reset();
	}

	//Turso3DUtils::ConvertModel("Data/Jack.mdl", "Data/jack.tmf");
	//Turso3DUtils::ConvertModel("Data/Box.mdl", "Data/box.tmf");
	//Turso3DUtils::ConvertModel("Data/Mushroom.mdl", "Data/mushroom.tmf");
}

Application::~Application()
{
}

bool Application::Initialize()
{
	if (!graphics) {
		return false;
	}
	graphics->SetVSync(false);

	// Create subsystems that depend on the application window / OpenGL
	renderer = std::make_unique<Renderer>(workQueue.get(), graphics.get());
	renderer->SetupShadowMaps(DIRECTIONAL_LIGHT_SIZE, LIGHT_ATLAS_SIZE, FMT_D16);

	debugRenderer = std::make_unique<DebugRenderer>(graphics.get());
	bloomRenderer = std::make_unique<BloomRenderer>(graphics.get());

	// Create the scene and camera.
	// Camera is created outside scene so it's not disturbed by scene clears
	camera = std::make_shared<Camera>();
	scene = std::make_shared<Scene>(workQueue.get(), graphics.get());

	// Define textures
	CreateTextures();
	OnFramebufferSize(graphics->RenderWidth(), graphics->RenderHeight());

	// Create scene
	CreateDefaultScene();

	return true;
}

void Application::Run()
{
	constexpr double fixedRate = 1.0 / 60.0;

	GLFWwindow* window = graphics->Window();

	// Main loop
	while (true) {
		if (glfwWindowShouldClose(window)) {
			break;
		}

		// Calculate delta time
		double now = glfwGetTime();
		deltaTime = now - timestamp;
		timestamp = now;

		Update(deltaTime);

		// Fixed time update
		deltaTimeAccumulator += deltaTime;
		if (deltaTimeAccumulator > 10.0) {
			// TODO: Send an event, FixedTimeReset
			deltaTimeAccumulator = 0;

		} else {
			while (deltaTimeAccumulator >= fixedRate) {
				FixedUpdate(fixedRate);
				deltaTimeAccumulator -= fixedRate;
			}
		}

		PostUpdate(deltaTime);
		Render(deltaTime);

		glfwPollEvents();

		ApplyFrameLimit();
	}
}

void Application::ApplyFrameLimit()
{
	if (frameLimit == 0) {
		return;
	}

	// Skip frame limiter if in fullscreen mode, vsync is on and the refresh rate matches the frame limit value.
	if (graphics->VSync() && graphics->FullscreenRefreshRate() == frameLimit) {
		return;
	}

	const double targetRate = 1.0 / (double)frameLimit;
	while (true) {
		double rate = glfwGetTime() - timestamp;
		if (rate < targetRate) {
			double d = (rate - targetRate);
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

void Application::CreateTextures()
{
	for (int i = 0; i < 2; ++i) {
		mrtFbo[i] = std::make_unique<FrameBuffer>();
		hdrBuffer[i] = std::make_unique<Texture>();
		normalBuffer[i] = std::make_unique<Texture>();
		depthStencilBuffer[i] = std::make_unique<Texture>();
	}

	ldrFbo = std::make_unique<FrameBuffer>();
	ldrBuffer = std::make_unique<Texture>();

	// Random noise texture for SSAO
	unsigned char noiseData[4 * 4 * 4];
	for (int i = 0; i < 4 * 4; ++i) {
		Vector3 noiseVec(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f);
		noiseVec.Normalize();

		noiseData[i * 4 + 0] = (unsigned char)(noiseVec.x * 127.0f + 128.0f);
		noiseData[i * 4 + 1] = (unsigned char)(noiseVec.y * 127.0f + 128.0f);
		noiseData[i * 4 + 2] = (unsigned char)(noiseVec.z * 127.0f + 128.0f);
		noiseData[i * 4 + 3] = 0;
	}

	ssaoFbo = std::make_unique<FrameBuffer>();
	ssaoTexture = std::make_unique<Texture>();

	ImageLevel noiseDataLevel(IntVector2(4, 4), FMT_RGBA8, &noiseData[0]);
	ssaoNoiseTexture = std::make_unique<Texture>();
	ssaoNoiseTexture->Define(TEX_2D, IntVector2(4, 4), FMT_RGBA8, false, 1, 1, &noiseDataLevel);
	ssaoNoiseTexture->DefineSampler(FILTER_POINT);
}

void Application::CreateDefaultScene()
{
	scene->Clear();
	Node* root = scene->GetRoot();

	Octree* octree = scene->GetOctree();
	//octree->Resize(BoundingBox(-1000.0f, 1000.0f), 0);

	LightEnvironment* lightEnvironment = scene->GetEnvironmentLighting();
	lightEnvironment->SetAmbientColor(Color(0.2f, 0.2f, 0.2f));

	camera->SetFarClip(1000.0f);
	camera->SetPosition(Vector3(-10.0f, 20.0f, 0.0f));

	// Set high quality shadows
	constexpr float biasMul = 1.25f;
	Material::SetGlobalShaderDefines("", "HQSHADOW");
	renderer->SetShadowDepthBiasMul(biasMul, biasMul);

	// =================================
	// Only for sample purposes

	ResourceCache* cache = ResourceCache::Instance();
#if 1
	for (int y = -55; y <= 55; ++y) {
		for (int x = -55; x <= 55; ++x) {
			StaticModel* object = root->CreateChild<StaticModel>();
			object->SetStatic(true);
			object->SetPosition(Vector3(10.5f * x, -0.05f, 10.5f * y));
			object->SetScale(Vector3(10.0f, 0.1f, 10.0f));
			object->SetModel(cache->LoadResource<Model>("box.tmf"));
			object->SetMaterial(cache->LoadResource<Material>("stone.xml"));
		}
	}

	for (unsigned i = 0; i < 10000; ++i) {
		StaticModel* object = root->CreateChild<StaticModel>();
		object->SetStatic(true);
		object->SetPosition(Vector3(Random() * 1000.0f - 500.0f, 0.0f, Random() * 1000.0f - 500.0f));
		object->SetScale(1.5f);
		object->SetModel(cache->LoadResource<Model>("mushroom.tmf"));
		object->SetMaterial(cache->LoadResource<Material>("mushroom.xml"));
		object->SetCastShadows(true);
		object->SetLodBias(2.0f);
		object->SetMaxDistance(600.0f);
	}

	{
		StaticModel* object = root->CreateChild<StaticModel>();
		//object->SetStatic(true);
		object->SetPosition(Vector3(-10.0f, 0.5f, 50.0f));
		Quaternion rot {};
		rot.FromEulerAngles(90.0f, 90.0f, 0.0f);
		object->SetRotation(rot);
		object->SetScale(10.0f);
		//object->SetModel(cache->LoadResource<Model>("mushroom.tmf"));
		object->SetModel(cache->LoadResource<Model>("box.tmf"));
		//object->SetModel(cache->LoadResource<Model>("jack.tmf"));
		object->SetMaterial(cache->LoadResource<Material>("mushroom.xml"));
		object->SetCastShadows(true);
		//object->SetLodBias(2.0f);
		object->SetMaxDistance(600.0f);

		model = object;
	}

	{
		StaticModel* object = root->CreateChild<StaticModel>();
		//object->SetStatic(true);
		object->SetPosition(Vector3(-15.0f, 0, 50.0f));
		object->SetModel(cache->LoadResource<Model>("plane.tmf"));
		object->SetMaterial(cache->LoadResource<Material>("plane.xml"));
		object->SetCastShadows(true);
		object->SetMaxDistance(600.0f);
	}

	{
		AnimatedModel* object = root->CreateChild<AnimatedModel>();
		//object->SetStatic(true);
		object->SetPosition(Vector3(Random() * 90.0f - 45.0f, 0.0f, Random() * 90.0f - 45.0f));
		object->SetRotation(Quaternion(Random(360.0f), Vector3::UP));
		object->SetModel(cache->LoadResource<Model>("jack.tmf"));
		object->SetCastShadows(true);
		object->SetMaxDistance(600.0f);

		AnimationState* state = object->AddAnimationState(cache->LoadResource<Animation>("jack_walk.ani"));
		state->SetWeight(1.0f);
		state->SetLooped(true);

		characterModel = object;
	}

	Vector3 quadrantCenters[] =
	{
		Vector3(-290.0f, 0.0f, -290.0f),
		Vector3(290.0f, 0.0f, -290.0f),
		Vector3(-290.0f, 0.0f, 290.0f),
		Vector3(290.0f, 0.0f, 290.0f),
	};

	std::vector<Light*> lights;

	for (unsigned i = 0; i < 100; ++i) {
		Light* light = root->CreateChild<Light>();
		light->SetStatic(true);
		light->SetLightType(LIGHT_POINT);
		light->SetCastShadows(true);
		Vector3 colorVec = 5.0f * Vector3(Random(), Random(), Random()).Normalized();
		light->SetColor(Color(colorVec.x, colorVec.y, colorVec.z, 1.0f));
		light->SetRange(50.0f);
		light->SetShadowMapSize(1024);
		light->SetShadowMaxDistance(200.0f);
		light->SetMaxDistance(0.0f);

		for (;;) {
			Vector3 newPos = quadrantCenters[i % 4] + Vector3(Random() * 500.0f - 250.0f, 10.0f, Random() * 500.0f - 250.0f);
			bool posOk = true;

			for (unsigned j = 0; j < lights.size(); ++j) {
				if ((newPos - lights[j]->Position()).Length() < 80.0f) {
					//posOk = false;
					break;
				}
			}

			if (posOk) {
				light->SetPosition(newPos);
				break;
			}
		}

		lights.push_back(light);
	}

	{
		Vector4 color = 100.0f * Vector4(1.0f, 1.0f, 0.5f, 1.0);

		Light* light = root->CreateChild<Light>();
		//light->SetStatic(true);
		light->SetLightType(LIGHT_DIRECTIONAL);
		light->SetCastShadows(true);
		light->SetColor(Color(color.Data()));
		light->SetDirection(Vector3(0.45f, -0.45f, 0.0f));
		//light->SetRange(600.0f);
		light->SetShadowMapSize(DIRECTIONAL_LIGHT_SIZE);
		light->SetShadowMaxDistance(1000.0f);
		light->SetMaxDistance(0.0f);
		light->SetEnabled(true);
	}

	{
		StaticModel* object = root->CreateChild<StaticModel>();
		object->SetStatic(true);
		object->SetPosition(Vector3(0.0f, 25.0f, 0.0f));
		object->SetScale(Vector3(1165.0f, 50.0f, 1.0f));
		object->SetModel(cache->LoadResource<Model>("box.tmf"));
		object->SetMaterial(cache->LoadResource<Material>("stone.xml"));
		object->SetCastShadows(true);
	}

	{
		StaticModel* object = root->CreateChild<StaticModel>();
		object->SetStatic(true);
		object->SetPosition(Vector3(0.0f, 25.0f, 0.0f));
		object->SetScale(Vector3(1.0f, 50.0f, 1165.0f));
		object->SetModel(cache->LoadResource<Model>("box.tmf"));
		object->SetMaterial(cache->LoadResource<Material>("stone.xml"));
		object->SetCastShadows(true);
	}
#endif
}

bool Application::IsKeyDown(int key)
{
	auto it = keyStates.find(key);
	if (it != keyStates.end()) {
		return (it->second == InputState::Pressed) || (it->second == InputState::Down);
	}
	return false;
}

bool Application::IsKeyPressed(int key)
{
	auto it = keyStates.find(key);
	if (it != keyStates.end()) {
		return (it->second == InputState::Pressed);
	}
	return false;
}

bool Application::IsMouseDown(int button)
{
	auto it = mouseButtonStates.find(button);
	if (it != mouseButtonStates.end()) {
		return (it->second == InputState::Pressed) || (it->second == InputState::Down);
	}
	return false;
}

// ==========================================================================================
void Application::OnKey(int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) {
		keyStates[key] = InputState::Pressed;
	} else if (action == GLFW_RELEASE) {
		keyStates[key] = InputState::Released;
	}
}

void Application::OnMouseButton(int button, int action, int mods)
{
	if (!cursorInside) {
		return;
	}
	if (action == GLFW_PRESS) {
		mouseButtonStates[button] = InputState::Pressed;
	} else if (action == GLFW_RELEASE) {
		mouseButtonStates[button] = InputState::Released;
	}
}

void Application::OnMouseMove(double xpos, double ypos)
{
	if (!cursorInside) {
		return;
	}
	Vector2 newPos {(float)xpos, (float)ypos};
	cursorSpeed = newPos - prevCursorPos;
	prevCursorPos = newPos;
}

void Application::OnMouseEnterLeave(bool entered)
{
	cursorInside = entered;
	if (entered) {
		captureMouse = false;

	} else {
		// Release all mouse button when leaving
		for (auto& it : mouseButtonStates) {
			InputState& state = it.second;
			if (state == InputState::Pressed || state == InputState::Down) {
				state = InputState::Up;
			}
		}
	}
}

void Application::OnFramebufferSize(int width, int height)
{
	IntVector2 sz {width, height};
	if (width <= 0 || height <= 0 || hdrBuffer[0]->Size2D() == sz) {
		return;
	}

	// Define the base rendertargets
	for (int i = 0; i < 2; ++i) {
		hdrBuffer[i]->Define(TEX_2D, sz, FMT_R11_G11_B10F, false, multiSample * i);
		hdrBuffer[i]->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		normalBuffer[i]->Define(TEX_2D, sz, FMT_RGBA8, false, multiSample * i);
		normalBuffer[i]->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
		depthStencilBuffer[i]->Define(TEX_2D, sz, FMT_D32, false, multiSample * i);
		depthStencilBuffer[i]->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		Texture* colors[] = {
			hdrBuffer[i].get(),
			normalBuffer[i].get()
		};
		mrtFbo[i]->Define(colors, std::size(colors), depthStencilBuffer[i].get());
	}

	ldrBuffer->Define(TEX_2D, sz, FMT_RGBA8, true);
	ldrBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	ldrFbo->Define(ldrBuffer.get(), depthStencilBuffer[0].get());

	// Bloom resources
	if (bloomRenderer) {
		bloomRenderer->UpdateBuffers(sz);
	}

	// SSAO resources
	ssaoTexture->Define(TEX_2D, IntVector2(width / 2, height / 2), FMT_RGBA8);
	ssaoTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	ssaoFbo->Define(ssaoTexture.get(), nullptr);

	camera->SetAspectRatio((float)width / (float)height);

	LOG_INFO("Framebuffer sized to: {:d}x{:d}", width, height);
}

void Application::OnWindowFocusChanged(int focused)
{
	// TODO
}

// ==========================================================================================
void Application::Update(double dt)
{
	// ==================================
	// Only for sample purposes
#if 1
	float moveSpeed = (IsKeyDown(GLFW_KEY_LEFT_SHIFT) || IsKeyDown(GLFW_KEY_RIGHT_SHIFT)) ? 50.0f : 5.0f;
	moveSpeed *= (IsKeyDown(GLFW_KEY_LEFT_ALT) || IsKeyDown(GLFW_KEY_RIGHT_ALT)) ? 0.25f : 1.0f;

	if (IsKeyDown(GLFW_KEY_W)) camera->Translate(Vector3::FORWARD * moveSpeed * (float)dt);
	if (IsKeyDown(GLFW_KEY_S)) camera->Translate(Vector3::BACK * moveSpeed * (float)dt);
	if (IsKeyDown(GLFW_KEY_A)) camera->Translate(Vector3::LEFT * moveSpeed * (float)dt);
	if (IsKeyDown(GLFW_KEY_D)) camera->Translate(Vector3::RIGHT * moveSpeed * (float)dt);

	GLFWwindow* window = graphics->Window();
	int mouseMode = glfwGetInputMode(window, GLFW_CURSOR);

	if (cursorInside && IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
		if (mouseMode != GLFW_CURSOR_DISABLED) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			captureMouse = false;
		} else if (!captureMouse) {
			captureMouse = true;
			cursorSpeed = Vector2::ZERO;
		}

		if (captureMouse) {
			camYaw += (cursorSpeed.x * 0.1f);
			camPitch += (cursorSpeed.y * 0.1f);
			camPitch = Clamp(camPitch, -90.0f, 90.0f);

			camera->SetRotation(Quaternion(camPitch, camYaw, 0.0f));
		}

	} else if (mouseMode != GLFW_CURSOR_NORMAL) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	if (IsKeyPressed(GLFW_KEY_F)) graphics->SetFullscreen(!graphics->IsFullscreen());

	{
		AnimatedModel* object = characterModel;
		AnimationState* state = object->AnimationStates()[0].get();
		state->AddTime(dt);
		object->Translate(Vector3::FORWARD * 2.0f * dt);

		// Rotate to avoid going outside the plane
		Vector3 pos = object->Position();
		if (pos.x < -45.0f || pos.x > 45.0f || pos.z < -45.0f || pos.z > 45.0f) {
			object->Yaw(45.0f * dt);
		}
	}

	if (model) {
		model->Yaw(25.0f * dt, TS_WORLD);

		if (IsKeyPressed(GLFW_KEY_R)) {
			model->RemoveSelf();
			model = nullptr;

			ResourceCache::Instance()->ClearUnused();
		}
	}

#endif

	Render(dt);
}

void Application::PostUpdate(double dt)
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

	cursorSpeed = Vector2::ZERO;
}

void Application::FixedUpdate(double dt)
{
}

void Application::Render(double dt)
{
	const IntRect viewRect {0, 0, hdrBuffer[0]->Width(), hdrBuffer[0]->Height()};

	// Collect geometries and lights in frustum.
	// Also set debug renderer to use the correct camera view.
	renderer->PrepareView(scene.get(), camera.get(), true, true, (float)dt);
	debugRenderer->SetView(camera.get());

	// Now render the scene, starting with shadowmaps and opaque geometries
	renderer->RenderShadowMaps();
	graphics->SetViewport(viewRect);

	// The default opaque shaders can write both color (first RT) and view-space normals (second RT).
	if (multiSample > 1) {
		graphics->SetFrameBuffer(mrtFbo[1].get());
		renderer->RenderOpaque();

		// Resolve MSAA
		graphics->Blit(mrtFbo[0].get(), viewRect, mrtFbo[1].get(), viewRect, true, false, FILTER_BILINEAR);
		graphics->Blit(mrtFbo[0].get(), viewRect, mrtFbo[1].get(), viewRect, false, true, FILTER_POINT);

	} else {
		graphics->SetFrameBuffer(mrtFbo[0].get());
		renderer->RenderOpaque();
	}

	Texture* color = hdrBuffer[0].get(); // Non-multisample HDR color texture
	Texture* normal = normalBuffer[0].get(); // Non-multisample normal texture
	Texture* depth = depthStencilBuffer[0].get(); // Non-multisample Depth texture

	// Apply bloom
	if (bloomRenderer) {
		bloomRenderer->Render(color);
		color = bloomRenderer->GetComposedTexture();
	}

	// Apply tonemap
	{
		graphics->SetFrameBuffer(ldrFbo.get());
		ShaderProgram* program = graphics->SetProgram("PostProcess/Tonemap.glsl", "", "");
		graphics->SetUniform(program, "exposure", 0.75f);
		graphics->SetTexture(0, color);
		graphics->DrawQuad();
	}

	// SSAO effect.
	// First sample the normals and depth buffer, then apply a blurred SSAO result that darkens the opaque geometry
#if 0
	{
		float farClip = camera->FarClip();
		float nearClip = camera->NearClip();
		Vector3 nearVec, farVec;
		camera->FrustumSize(nearVec, farVec);

		float width = viewRect.Width();
		float height = viewRect.Height();

		ShaderProgram* program = graphics->SetProgram("PostProcess/SSAO.glsl", "", "");
		graphics->SetFrameBuffer(ssaoFbo.get());
		graphics->SetViewport(IntRect(0, 0, ssaoTexture->Width(), ssaoTexture->Height()));
		graphics->SetUniform(program, "noiseInvSize", Vector2(ssaoTexture->Width() / 4.0f, ssaoTexture->Height() / 4.0f));
		graphics->SetUniform(program, "screenInvSize", Vector2(1.0f / width, 1.0f / height));
		graphics->SetUniform(program, "frustumSize", Vector4(farVec, height / width));
		graphics->SetUniform(program, "aoParameters", Vector4(0.15f, 1.0f, 0.025f, 0.15f));
		graphics->SetUniform(program, "depthReconstruct", Vector2(farClip / (farClip - nearClip), -nearClip / (farClip - nearClip)));
		graphics->SetTexture(0, depth);
		graphics->SetTexture(1, normal);
		graphics->SetTexture(2, ssaoNoiseTexture.get());
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();
		graphics->SetTexture(1, nullptr);
		graphics->SetTexture(2, nullptr);

		program = graphics->SetProgram("PostProcess/SSAOBlur.glsl", "", "");
		graphics->SetFrameBuffer(ldrFbo.get());
		graphics->SetViewport(IntRect(0, 0, width, height));
		graphics->SetUniform(program, "blurInvSize", Vector2(1.0f / ssaoTexture->Width(), 1.0f / ssaoTexture->Height()));
		graphics->SetTexture(0, ssaoTexture.get());
		graphics->SetRenderState(BLEND_SUBTRACT, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();
		graphics->SetTexture(0, nullptr);
	}
#endif

	// Render alpha geometry.
	// Now only the color rendertarget is needed
	graphics->SetFrameBuffer(ldrFbo.get());
	graphics->SetViewport(viewRect);
	renderer->RenderAlpha();

	// Optional render of debug geometry
	bool drawDebug = false;
	if (drawDebug) {
		Octree* octree = scene->GetOctree();

		// Raycast into the scene using the camera forward vector.
		// If has a hit, draw a small debug sphere at the hit location.
		{
			Ray cameraRay(camera->WorldPosition(), camera->WorldDirection());
			RaycastResult res = scene->GetOctree()->RaycastSingle(cameraRay, Drawable::FLAG_GEOMETRY);
			if (res.drawable) {
				debugRenderer->AddSphere(Sphere(res.position, 0.05f), Color::WHITE, true);
			}
		}

		renderer->RenderDebug(debugRenderer.get());
		debugRenderer->Render();
	}

	// Blit rendered contents to backbuffer now before presenting
	graphics->Blit(nullptr, viewRect, ldrFbo.get(), viewRect, true, false, FILTER_POINT);
	graphics->Present();
}
