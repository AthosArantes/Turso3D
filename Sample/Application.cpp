#include "Application.h"
#include "BlurRenderer.h"
#include "BloomRenderer.h"
#include "SSAORenderer.h"
#include "ModelConverter.h"
#include "RmlUi/RmlSystem.h"
#include "RmlUi/RmlRenderer.h"
#include "RmlUi/RmlFile.h"
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/Graphics/ShaderProgram.h>
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
#include <RmlUi/Core.h>
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
	frameLimit(0),
	cursorInside(false),
	camYaw(0),
	camPitch(0),
	useOcclusion(true),
	renderDebug(false),
	character(nullptr)
{
	// Create subsystems that don't depend on the application window / OpenGL context
	workQueue = std::make_unique<WorkQueue>();
	workQueue->CreateWorkerThreads(2);

	ResourceCache* cache = ResourceCache::Instance();
	cache->AddResourceDir((std::filesystem::current_path() / "Shaders").string());
	cache->AddResourceDir((std::filesystem::current_path() / "Data").string());

	// Initialize graphics
	graphics = std::make_unique<Graphics>();

	if (graphics->Initialize("Turso3D renderer test", 1600, 900)) {
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

	blurRenderer = std::make_unique<BlurRenderer>();
	bloomRenderer = std::make_unique<BloomRenderer>();
	ssaoRenderer = std::make_unique<SSAORenderer>();
}

Application::~Application()
{
	// Shutdown RmlUi
	Rml::Shutdown();
	rmlRenderer.reset();
	rmlSystem.reset();
	rmlFile.reset();
}

bool Application::Initialize()
{
	if (!graphics) {
		return false;
	}
	graphics->SetVSync(true);

	// Create subsystems that depend on the application window / OpenGL
	renderer = std::make_unique<Renderer>(workQueue.get(), graphics.get());
	renderer->SetupShadowMaps(DIRECTIONAL_LIGHT_SIZE, LIGHT_ATLAS_SIZE, FORMAT_D32_SFLOAT_PACK32);
	debugRenderer = std::make_unique<DebugRenderer>(graphics.get());

	blurRenderer->Initialize(graphics.get());
	bloomRenderer->Initialize(graphics.get());
	ssaoRenderer->Initialize(graphics.get());

	tonemapProgram = graphics->CreateProgram("PostProcess/Tonemap.glsl", "", "");
	uTonemapExposure = tonemapProgram->Uniform(StringHash {"exposure"});

	guiProgram = graphics->CreateProgram("PostProcess/GuiCompose.glsl", "", "");

	// Initialize RmlUi
	rmlFile = std::make_unique<RmlFile>();
	rmlSystem = std::make_unique<RmlSystem>();
	rmlRenderer = std::make_unique<RmlRenderer>(graphics.get());
	Rml::SetFileInterface(rmlFile.get());
	Rml::SetSystemInterface(rmlSystem.get());
	Rml::SetRenderInterface(rmlRenderer.get());
	Rml::Initialise();

	// Load base fonts
	constexpr const char* fonts[] = {
		"ui/fonts/FiraGO-Bold.ttf",
		"ui/fonts/FiraGO-BoldItalic.ttf",
		"ui/fonts/FiraGO-Book.ttf",
		"ui/fonts/FiraGO-BookItalic.ttf",
		"ui/fonts/FiraGO-ExtraBold.ttf",
		"ui/fonts/FiraGO-ExtraBoldItalic.ttf",
		"ui/fonts/FiraGO-Heavy.ttf",
		"ui/fonts/FiraGO-HeavyItalic.ttf",
		"ui/fonts/FiraGO-Italic.ttf",
		"ui/fonts/FiraGO-Light.ttf",
		"ui/fonts/FiraGO-LightItalic.ttf",
		"ui/fonts/FiraGO-Medium.ttf",
		"ui/fonts/FiraGO-MediumItalic.ttf",
		"ui/fonts/FiraGO-Regular.ttf",
		"ui/fonts/FiraGO-SemiBold.ttf",
		"ui/fonts/FiraGO-SemiBoldItalic.ttf"
	};
	for (int i = 0; i < std::size(fonts); ++i) {
		Rml::LoadFontFace(fonts[i]);
	}

	// Create main rml context
	mainContext = Rml::CreateContext("main", Rml::Vector2i {graphics->RenderWidth(), graphics->RenderHeight()});
	auto doc = mainContext->LoadDocument("ui/demo.rml");
	doc->Show();

	// Create the scene and camera.
	// Camera is created outside scene so it's not disturbed by scene clears
	camera = std::make_shared<Camera>();
	scene = std::make_shared<Scene>(workQueue.get(), graphics.get());

	// Define textures
	CreateTextures();
	OnFramebufferSize(graphics->RenderWidth(), graphics->RenderHeight());

	// Create scene
	CreateDefaultScene();
	//CreateSpheresScene();
	CreateThousandMushroomScene();
	CreateWalkingCharacter();

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

	guiFbo = std::make_unique<FrameBuffer>();
	guiTexture = std::make_unique<Texture>();
}

void Application::CreateDefaultScene()
{
	ResourceCache* cache = ResourceCache::Instance();

	scene->Clear();
	Node* root = scene->GetRoot();

	Octree* octree = scene->GetOctree();
	//octree->Resize(BoundingBox(-1000.0f, 1000.0f), 0);

	LightEnvironment* lightEnvironment = scene->GetEnvironmentLighting();
	{
		std::shared_ptr<Texture> iemTex = cache->LoadResource<Texture>("ibl/daysky_iem.dds");
		iemTex->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		std::shared_ptr<Texture> pmremTex = cache->LoadResource<Texture>("ibl/daysky_pmrem.dds");
		pmremTex->DefineSampler(FILTER_TRILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		std::shared_ptr<Texture> brdfTex = cache->LoadResource<Texture>("ibl/brdf.dds");
		brdfTex->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		lightEnvironment->SetIBLMaps(iemTex, pmremTex, brdfTex);
	}

	camera->SetFarClip(1000.0f);
	camera->SetPosition(Vector3(-10.0f, 20.0f, 0.0f));

	// Set high quality shadows
	constexpr float biasMul = 1.25f;
	Material::SetGlobalShaderDefines("", "HQSHADOW");
	renderer->SetShadowDepthBiasMul(biasMul, biasMul);

#if 1
	// Sun
	Light* light = root->CreateChild<Light>();
	//light->SetStatic(true);
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);

	Vector3 color = 100.0f * Vector3 {1.0f, 1.0f, 0.6f};
	light->SetColor(Color(color.x, color.y, color.z, 1.0f));
	light->SetDirection(Vector3 {0.45f, -0.45f, 0.30f});
	//light->SetRange(600.0f);
	light->SetShadowMapSize(DIRECTIONAL_LIGHT_SIZE);
	light->SetShadowMaxDistance(50.0f);
	light->SetMaxDistance(0.0f);
	light->SetEnabled(true);

	//static_cast<LightDrawable*>(light->GetDrawable())->SetAutoFocus(true);

#elif 0

	// Moon
	Light* light = root->CreateChild<Light>();
	//light->SetStatic(true);
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);

	Vector3 color = 100.0f * Vector3(0.4f, 0.4f, 1.0f);
	light->SetColor(Color(color.x, color.y, color.z, 1.0f));
	light->SetDirection(Vector3(0.15f, -0.15f, 0.30f));
	//light->SetRange(600.0f);
	light->SetShadowMapSize(DIRECTIONAL_LIGHT_SIZE);
	light->SetShadowMaxDistance(50.0f);
	light->SetMaxDistance(0.0f);
	light->SetEnabled(true);

	//static_cast<LightDrawable*>(light->GetDrawable())->SetAutoFocus(true);
#endif

}

void Application::CreateSpheresScene()
{
	ResourceCache* cache = ResourceCache::Instance();
	Node* root = scene->GetRoot();

	std::shared_ptr<Material> baseMaterial = Material::GetDefault();

	constexpr int count = 7;
	constexpr float v = 1.0f / (count - 1);

	Vector3 basePos {-0.4f, 0.6f, -1.0f};

	for (int y = 0; y < count; ++y) {
		for (int x = 0; x < count; ++x) {
			constexpr float size = 0.1f;
			constexpr float pos = size * 1.2f;

			StaticModel* object = root->CreateChild<StaticModel>();
			object->SetStatic(true);
			object->SetCastShadows(true);
			object->SetPosition(basePos + Vector3 {pos * x, pos * y, 0.0f});
			object->SetScale(size);
			object->SetModel(cache->LoadResource<Model>("sphere.tmf"));

			float roughness = x * v;
			float metallic = y * v;

			std::shared_ptr<Material> mtl = baseMaterial->Clone();
			mtl->SetUniform("BaseColor", Vector4 {1.0f, 1.0f, 1.0f, 1.0f});
			mtl->SetUniform("AoRoughMetal", Vector4 {1.0f, roughness, metallic, 0.0f});

			object->SetMaterial(mtl);
		}
	}

	bool lights = true;
	if (lights) {
		for (unsigned i = 0; i < 4; ++i) {
			Light* light = root->CreateChild<Light>();
			//light->SetStatic(true);
			light->SetLightType(LIGHT_POINT);
			//light->SetCastShadows(true);
			light->SetPosition(Vector3 {Random() * 2.0f - 1.0f, Random() * 2.0f - 2.0f, -1.0f} *3.0f);

			light->SetColor(Color::WHITE() * 10.0f);
			light->SetRange(10.0f);
			light->SetShadowMapSize(1024);
			light->SetShadowMaxDistance(5.0f);
			light->SetMaxDistance(50.0f);
		}
	}

	camera->SetPosition(Vector3(0.0f, 1.0f, -2.2f));
}

void Application::CreateThousandMushroomScene()
{
	ResourceCache* cache = ResourceCache::Instance();
	Node* root = scene->GetRoot();

	std::shared_ptr<Model> floorModel = cache->LoadResource<Model>("plane.tmf");
	std::shared_ptr<Material> floorMaterial = cache->LoadResource<Material>("bricks/bricks075a.xml");

	std::shared_ptr<Model> mushroomModel = cache->LoadResource<Model>("mushroom.tmf");
	std::shared_ptr<Material> mushroomMaterial = cache->LoadResource<Material>("mushroom.xml");

	for (int y = -55; y <= 55; ++y) {
		for (int x = -55; x <= 55; ++x) {
			StaticModel* floor = root->CreateChild<StaticModel>();
			floor->SetStatic(true);
			floor->SetPosition(Vector3(10.5f * x, 0.0f, 10.5f * y));
			floor->SetScale(Vector3(10.0f, 1.0f, 10.0f));
			floor->SetModel(floorModel);
			floor->SetMaterial(floorMaterial);

			for (int cx = -1; cx <= 1; ++cx) {
				for (int cy = -1; cy <= 1; ++cy) {
					StaticModel* object = root->CreateChild<StaticModel>();
					object->SetStatic(true);
					object->SetPosition(Vector3(10.5f * x + cx * 2, 0.0f, 10.5f * y + cy * 2));
					object->SetRotation(Quaternion(0, Random() * 360, 0));
					object->SetScale(0.5f);
					object->SetModel(mushroomModel);
					object->SetMaterial(mushroomMaterial);
					object->SetCastShadows(true);
					//object->SetLodBias(2000.0f);
					object->SetMaxDistance(0.0f);
				}
			}
		}
	}

	// Crate huge walls
	/*{
		std::shared_ptr<Model> boxModel = cache->LoadResource<Model>("box.tmf");

		float rotate[] = {0.0f, 90.f};
		for (int i = 0; i < 2; ++i) {
			StaticModel* wall = root->CreateChild<StaticModel>();
			wall->SetStatic(true);
			wall->SetPosition(Vector3 {0.0f, 14.0f, 0.0f});
			wall->SetScale(Vector3(1000.0f, 30.0f, 0.1f));
			wall->Rotate(Quaternion(0.0f, rotate[i], 0.0f));
			wall->SetModel(boxModel);
			wall->SetMaterial(floorMaterial);
			wall->SetCastShadows(true);
		}
	}//*/
}

void Application::CreateWalkingCharacter()
{
	ResourceCache* cache = ResourceCache::Instance();
	Node* root = scene->GetRoot();

	std::shared_ptr<Model> charModel = cache->LoadResource<Model>("jack.tmf");

	character = root->CreateChild<AnimatedModel>();
	character->SetStatic(false);
	character->SetModel(charModel);
	character->SetCastShadows(true);
	character->SetMaxDistance(600.0f);

	AnimationState* state = character->AddAnimationState(cache->LoadResource<Animation>("Jack_Walk.ani"));
	state->SetWeight(1.0f);
	state->SetLooped(true);
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

	camera->SetAspectRatio((float)width / (float)height);

	// Define the base rendertargets
	for (int i = 0; i < 2; ++i) {
		hdrBuffer[i]->Define(TARGET_2D, sz, FORMAT_RG11B10_UFLOAT_PACK32, multiSample * i);
		hdrBuffer[i]->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		normalBuffer[i]->Define(TARGET_2D, sz, FORMAT_RGB16_SNORM_PACK16, multiSample * i);
		normalBuffer[i]->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		depthStencilBuffer[i]->Define(TARGET_2D, sz, FORMAT_D32_SFLOAT_PACK32, multiSample * i);
		depthStencilBuffer[i]->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		Texture* colors[] = {
			hdrBuffer[i].get(),
			normalBuffer[i].get()
		};
		mrtFbo[i]->Define(colors, std::size(colors), depthStencilBuffer[i].get());

		if (multiSample == 1) {
			break;
		}
	}

	ldrBuffer->Define(TARGET_2D, sz, FORMAT_RGBA8_SRGB_PACK32);
	ldrBuffer->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	ldrFbo->Define(ldrBuffer.get(), depthStencilBuffer[0].get());

	guiTexture->Define(TARGET_2D, sz, ldrBuffer->Format());
	guiTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
	guiFbo->Define(guiTexture.get(), nullptr);

	blurRenderer->UpdateBuffers(sz / 2, ldrBuffer->Format(), IntVector2::ZERO(), 4);

	// Bloom resources
	if (bloomRenderer) {
		bloomRenderer->UpdateBuffers(sz, hdrBuffer[0]->Format());
	}

	// SSAO resources
	if (ssaoRenderer) {
		ssaoRenderer->UpdateBuffers(sz);
	}

	if (rmlRenderer) {
		rmlRenderer->UpdateBuffers(sz, 4);

		mainContext->SetDimensions(Rml::Vector2i(sz.x, sz.y));
		mainContext->Update();
	}

	LOG_INFO("Framebuffer sized to: {:d}x{:d}", width, height);
}

void Application::OnWindowFocusChanged(int focused)
{
	// TODO
}

// ==========================================================================================
void Application::Update(double dt)
{
	const float dtf = static_cast<float>(dt);

	double delay = mainContext->GetNextUpdateDelay();
	if (delay <= dt) {
		mainContext->Update();
	}

	// ==================================
	// Only for sample purposes
#if 1
	float moveSpeed = (IsKeyDown(GLFW_KEY_LEFT_SHIFT) || IsKeyDown(GLFW_KEY_RIGHT_SHIFT)) ? 50.0f : 5.0f;
	moveSpeed *= (IsKeyDown(GLFW_KEY_LEFT_ALT) || IsKeyDown(GLFW_KEY_RIGHT_ALT)) ? 0.25f : 1.0f;

	Vector3 camTranslation = Vector3::ZERO();
	if (IsKeyDown(GLFW_KEY_W)) camTranslation += Vector3::FORWARD();
	if (IsKeyDown(GLFW_KEY_S)) camTranslation += Vector3::BACK();
	if (IsKeyDown(GLFW_KEY_A)) camTranslation += Vector3::LEFT();
	if (IsKeyDown(GLFW_KEY_D)) camTranslation += Vector3::RIGHT();
	camera->Translate(camTranslation * moveSpeed * dtf);

	GLFWwindow* window = graphics->Window();
	int mouseMode = glfwGetInputMode(window, GLFW_CURSOR);

	if (cursorInside && IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
		if (mouseMode != GLFW_CURSOR_DISABLED) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			captureMouse = false;
		} else if (!captureMouse) {
			captureMouse = true;
			cursorSpeed = Vector2::ZERO();
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

	if (IsKeyPressed(GLFW_KEY_1)) useOcclusion = !useOcclusion;
	if (IsKeyPressed(GLFW_KEY_2)) renderDebug = !renderDebug;
	if (IsKeyPressed(GLFW_KEY_F)) graphics->SetFullscreen(!graphics->IsFullscreen());
	if (IsKeyPressed(GLFW_KEY_V)) graphics->SetVSync(!graphics->VSync());

	if (character) {
		AnimationState* state = character->AnimationStates()[0].get();
		state->AddTime(dtf);

		character->Translate(Vector3::FORWARD() * 2.0f * dtf);

		// Rotate to avoid going outside the plane
		Vector3 pos = character->Position();
		if (pos.x < -45.0f || pos.x > 45.0f || pos.z < -45.0f || pos.z > 45.0f) {
			character->Yaw(46.0f * dtf);
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

	cursorSpeed = Vector2::ZERO();
}

void Application::FixedUpdate(double dt)
{
}

void Application::Render(double dt)
{
	const IntRect viewRect {0, 0, hdrBuffer[0]->Width(), hdrBuffer[0]->Height()};

	// Collect geometries and lights in frustum.
	// Also set debug renderer to use the correct camera view.
	renderer->PrepareView(scene.get(), camera.get(), true, useOcclusion, (float)dt);
	debugRenderer->SetView(camera.get());

	// Now render the scene, starting with shadowmaps and opaque geometries
	renderer->RenderShadowMaps();
	graphics->SetViewport(viewRect);

	// The default opaque shaders can write both color (first RT) and view-space normals (second RT).
	mrtFbo[(multiSample > 1) ? 1 : 0]->Bind();
	renderer->RenderOpaque();
	renderer->RenderAlpha();

	// Resolve MSAA
	if (multiSample > 1) {
		graphics->Blit(mrtFbo[0].get(), viewRect, mrtFbo[1].get(), viewRect, true, false, FILTER_BILINEAR);
		graphics->Blit(mrtFbo[0].get(), viewRect, mrtFbo[1].get(), viewRect, false, true, FILTER_POINT);
	}

	Texture* color = hdrBuffer[0].get(); // Resolved HDR color texture
	Texture* normal = normalBuffer[0].get(); // Resolved normal texture
	Texture* depth = depthStencilBuffer[0].get(); // Resolved Depth texture

	// HDR Bloom
	if (bloomRenderer) {
		TURSO3D_GL_MARKER("Bloom");

		bloomRenderer->Render(color, 0.03f);
		color = bloomRenderer->GetTexture();
	}

	// Tonemap
	{
		TURSO3D_GL_MARKER("Tonemap");

		ldrFbo->Bind();
		tonemapProgram->Bind();

		graphics->SetUniform(uTonemapExposure, 1.0f);
		color->Bind(0);

		graphics->SetViewport(viewRect);
		graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
		graphics->DrawQuad();
	}

	// SSAO
	if (ssaoRenderer) {
		TURSO3D_GL_MARKER("SSAO");

		ssaoRenderer->Render(camera.get(), normal, depth, ldrFbo.get(), viewRect);
	}

	// Optional render of debug geometry
	if (renderDebug) {
#if 0
		// Raycast into the scene using the camera forward vector.
		// If has a hit, draw a small debug sphere at the hit location.
		{
			Octree* octree = scene->GetOctree();
			Ray cameraRay(camera->WorldPosition(), camera->WorldDirection());
			RaycastResult res = scene->GetOctree()->RaycastSingle(cameraRay, Drawable::FLAG_GEOMETRY);
			if (res.drawable) {
				debugRenderer->AddSphere(Sphere(res.position, 0.05f), Color::WHITE(), true);
			}
		}
#endif

		renderer->RenderDebug(debugRenderer.get());
		debugRenderer->Render();
	}

	graphics->SetViewport(viewRect);

	// RmlUi
	{
		TURSO3D_GL_MARKER("Rml Ui");

		rmlRenderer->BeginRender();
		mainContext->Render();
		rmlRenderer->EndRender();
	}

	graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);

	// Blur scene for UI transparent backgrounds
	{
		TURSO3D_GL_MARKER("Scene Blur");

		blurRenderer->Downsample(ldrBuffer.get());
		blurRenderer->Upsample();

		graphics->SetViewport(viewRect);
		graphics->Blit(guiFbo.get(), viewRect, blurRenderer->GetFramebuffer(), IntRect {IntVector2::ZERO(), blurRenderer->GetTexture()->Size2D()}, true, false, FILTER_BILINEAR);
	}

	// Compose UI
	{
		TURSO3D_GL_MARKER("UI Compose");

		guiProgram->Bind();

		ldrBuffer->Bind(0);
		guiTexture->Bind(1);
		rmlRenderer->GetTexture()->Bind(2);
		rmlRenderer->GetMaskTexture()->Bind(3);

		FrameBuffer::Unbind();
		graphics->DrawQuad();
	}

	graphics->Present();
}
