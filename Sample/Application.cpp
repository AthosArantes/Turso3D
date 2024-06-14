#include "Application.h"
#include "Components/BloomRenderer.h"
#include "Components/BlurRenderer.h"
#include "Components/SSAORenderer.h"
#include "Components/TonemapRenderer.h"
#include "UiManager.h"
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/RenderBuffer.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Math/Math.h>
#include <Turso3D/Math/Random.h>
#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Renderer/Animation.h>
#include <Turso3D/Renderer/AnimationState.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Light.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/Renderer.h>
#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Scene/Scene.h>
#include <GLFW/glfw3.h>
#include <filesystem>

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
	cursorPos(Vector2::ZERO()),
	cursorSpeed(Vector2::ZERO()),
	camRotation(Vector2::ZERO()),
	useOcclusion(true),
	renderDebug(false),
	character(nullptr)
{
	workQueue->CreateWorkerThreads(2);

	ResourceCache* cache = ResourceCache::Instance();
	cache->AddResourceDir((std::filesystem::current_path() / "Shaders").string());
	cache->AddResourceDir((std::filesystem::current_path() / "Data").string());

	blurRenderer = std::make_unique<BlurRenderer>();
	bloomRenderer = std::make_unique<BloomRenderer>();
	aoRenderer = std::make_unique<SSAORenderer>();
	tonemapRenderer = std::make_unique<TonemapRenderer>();

	uiManager = std::make_unique<UiManager>();
}

Application::~Application()
{
}

bool Application::Initialize()
{
	if (!ApplicationBase::Initialize()) {
		return false;
	}
	graphics->SetVSync(true);

	// Configure renderer
	renderer->SetupShadowMaps(DIRECTIONAL_LIGHT_SIZE, LIGHT_ATLAS_SIZE, FORMAT_D32_SFLOAT_PACK32);

	blurRenderer->Initialize(graphics.get());
	bloomRenderer->Initialize(graphics.get());
	aoRenderer->Initialize(graphics.get());
	tonemapRenderer->Initialize(graphics.get());

	uiManager->Initialize(graphics.get());

	// Create the scene and camera.
	// Camera is created outside scene so it's not disturbed by scene clears
	camera = std::make_shared<Camera>();
	scene = std::make_shared<Scene>(workQueue.get(), graphics.get());

	// Define textures
	CreateTextures();
	OnFramebufferSize(graphics->RenderWidth(), graphics->RenderHeight());

	// Create scene
	SetupEnvironmentLighting();
	//CreateSpheresScene();
	CreateThousandMushroomScene();
	CreateWalkingCharacter();
	//CreateHugeWalls();

	return true;
}

void Application::OnMouseMove(double xpos, double ypos)
{
	if (!IsMouseInsideWindow() || !IsWindowFocused()) {
		return;
	}
	Vector2 pos {static_cast<float>(xpos), static_cast<float>(ypos)};
	cursorSpeed += pos - cursorPos;
	cursorPos = pos;
}

void Application::OnFramebufferSize(int width, int height)
{
	IntVector2 sz {width, height};
	if (sz.x <= 0 || sz.y <= 0 || sz == colorBuffer->Size2D()) {
		return;
	}

	camera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));

	constexpr TextureAddressMode clamp = ADDRESS_CLAMP;
	constexpr TextureFilterMode nearest = FILTER_POINT;

	colorBuffer->Define(TARGET_2D, sz, FORMAT_RG11B10_UFLOAT_PACK32);
	colorBuffer->DefineSampler(nearest, clamp, clamp, clamp);
	normalBuffer->Define(TARGET_2D, sz, FORMAT_RGBA16_UNORM_PACK16);
	normalBuffer->DefineSampler(nearest, clamp, clamp, clamp);
	depthBuffer->Define(TARGET_2D, sz, FORMAT_D32_SFLOAT_PACK32);
	depthBuffer->DefineSampler(nearest, clamp, clamp, clamp);

	// Define render targets
	if (multiSample > 1) {
		colorRbo->Define(sz, colorBuffer->Format(), multiSample);
		normalRbo->Define(sz, normalBuffer->Format(), multiSample);
		depthRbo->Define(sz, depthBuffer->Format(), multiSample);

		// Define dst/src framebuffers for multisample resolve
		colorFbo[0]->Define(colorBuffer.get(), nullptr);
		normalFbo[0]->Define(normalBuffer.get(), nullptr);
		depthFbo[0]->Define(nullptr, depthBuffer.get());

		colorFbo[1]->Define(colorRbo.get(), nullptr);
		normalFbo[1]->Define(normalRbo.get(), nullptr);
		depthFbo[1]->Define(nullptr, depthRbo.get());

		RenderBuffer* mrt[] = {colorRbo.get(), normalRbo.get()};
		hdrFbo->Define(mrt, 2, depthRbo.get());

	} else {
		// Release render buffers and the resolve framebuffers
		colorRbo = std::make_unique<RenderBuffer>();
		normalRbo = std::make_unique<RenderBuffer>();
		depthRbo = std::make_unique<RenderBuffer>();

		for (int i = 0; i < 2; ++i) {
			colorFbo[i] = std::make_unique<FrameBuffer>();
			normalFbo[i] = std::make_unique<FrameBuffer>();
			depthFbo[i] = std::make_unique<FrameBuffer>();
		}

		Texture* mrt[] = {colorBuffer.get(), normalBuffer.get()};
		hdrFbo->Define(mrt, 2, depthBuffer.get());
	}

	ldrBuffer->Define(TARGET_2D, sz, FORMAT_RGBA8_SRGB_PACK32);
	ldrBuffer->DefineSampler(nearest, clamp, clamp, clamp);
	ldrFbo->Define(ldrBuffer.get(), depthBuffer.get());

	blurRenderer->UpdateBuffers(sz / 2, ldrBuffer->Format(), IntVector2::ZERO(), 4);

	// Bloom resources
	if (bloomRenderer) {
		bloomRenderer->UpdateBuffers(sz, colorBuffer->Format());
	}

	// SSAO resources
	if (aoRenderer) {
		aoRenderer->UpdateBuffers(sz);
	}

	// UI resources
	if (uiManager) {
		uiManager->UpdateBuffers(sz);
	}

	LOG_INFO("Framebuffer sized to: {:d}x{:d}", width, height);
}

// ==========================================================================================
void Application::CreateTextures()
{
	colorBuffer = std::make_unique<Texture>();
	normalBuffer = std::make_unique<Texture>();
	depthBuffer = std::make_unique<Texture>();
	colorRbo = std::make_unique<RenderBuffer>();
	normalRbo = std::make_unique<RenderBuffer>();
	depthRbo = std::make_unique<RenderBuffer>();
	hdrFbo = std::make_unique<FrameBuffer>();

	for (int i = 0; i < 2; ++i) {
		colorFbo[i] = std::make_unique<FrameBuffer>();
		normalFbo[i] = std::make_unique<FrameBuffer>();
		depthFbo[i] = std::make_unique<FrameBuffer>();
	}

	ldrBuffer = std::make_unique<Texture>();
	ldrFbo = std::make_unique<FrameBuffer>();
}

void Application::SetupEnvironmentLighting()
{
	ResourceCache* cache = ResourceCache::Instance();

	scene->Clear();

	Node* root = scene->GetRoot();
	Octree* octree = scene->GetOctree();

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

	Vector3 color = 1000.0f * Vector3 {1.0f, 1.0f, 0.6f};
	light->SetColor(Color(color.x, color.y, color.z, 1.0f));
	light->SetDirection(Vector3 {0.45f, -0.45f, 0.30f});
	//light->SetRange(600.0f);
	light->SetShadowMapSize(DIRECTIONAL_LIGHT_SIZE);
	light->SetShadowMaxDistance(50.0f);
	light->SetMaxDistance(0.0f);
	light->SetEnabled(true);
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
		Vector3 lightPositions[] = {
			Vector3(-1.0f, 1.0f, 1.0f),
			Vector3(1.0f, 1.0f, 1.0f),
			Vector3(-1.0f, -1.0f, 1.0f),
			Vector3(1.0f, -1.0f, 1.0f)
		};

		for (unsigned i = 0; i < 4; ++i) {
			Light* light = root->CreateChild<Light>();
			light->SetPosition(lightPositions[i]);
			//light->SetStatic(true);
			light->SetLightType(LIGHT_POINT);
			//light->SetCastShadows(true);
			//light->SetPosition(Vector3 {Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, -1.0f} * 3.0f);
			light->SetColor(Color::WHITE() * 10.0f);
			light->SetRange(5.0f);
			light->SetShadowMapSize(512);
			light->SetShadowMaxDistance(10.0f);
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
			floor->SetPosition(Vector3(10.5f * x, 0.0f, 10.5f * y));
			floor->SetScale(Vector3(10.0f, 1.0f, 10.0f));
			floor->SetStatic(true);
			floor->SetModel(floorModel);
			floor->SetMaterial(floorMaterial);

			for (int cx = -1; cx <= 1; ++cx) {
				for (int cy = -1; cy <= 1; ++cy) {
					StaticModel* mushroom = root->CreateChild<StaticModel>();
					mushroom->SetPosition(Vector3(10.5f * x + cx * 2, 0.0f, 10.5f * y + cy * 2));
					mushroom->SetRotation(Quaternion(0, Random() * 360, 0));
					mushroom->SetStatic(true);
					mushroom->SetScale(0.5f);
					mushroom->SetModel(mushroomModel);
					mushroom->SetMaterial(mushroomMaterial);
					mushroom->SetCastShadows(true);
				}
			}
		}
	}
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

void Application::CreateHugeWalls()
{
	ResourceCache* cache = ResourceCache::Instance();
	Node* root = scene->GetRoot();

	std::shared_ptr<Model> boxModel = cache->LoadResource<Model>("box.tmf");
	std::shared_ptr<Material> baseMaterial = Material::GetDefault();

	float rotate[] = {0.0f, 90.f};
	for (int i = 0; i < 2; ++i) {
		StaticModel* wall = root->CreateChild<StaticModel>();
		wall->SetStatic(true);
		wall->SetPosition(Vector3 {0.0f, 14.0f, 0.0f});
		wall->SetScale(Vector3(1000.0f, 30.0f, 0.1f));
		wall->Rotate(Quaternion(0.0f, rotate[i], 0.0f));
		wall->SetModel(boxModel);
		wall->SetMaterial(baseMaterial);
		wall->SetCastShadows(true);
	}
}

// ==========================================================================================
void Application::Update(double dt)
{
	GLFWwindow* window = graphics->Window();
	const float dtf = static_cast<float>(dt);

	// Ui
	if (uiManager) {
		uiManager->Update(dt);
	}

	// Camera move
	{
		float moveSpeed = (IsKeyDown(GLFW_KEY_LEFT_SHIFT) || IsKeyDown(GLFW_KEY_RIGHT_SHIFT)) ? 15.0f : 2.0f;
		moveSpeed *= (IsKeyDown(GLFW_KEY_LEFT_ALT) || IsKeyDown(GLFW_KEY_RIGHT_ALT)) ? 0.25f : 1.0f;

		Vector3 cam_tr = Vector3::ZERO();
		if (IsKeyDown(GLFW_KEY_W)) cam_tr += Vector3::FORWARD();
		if (IsKeyDown(GLFW_KEY_S)) cam_tr += Vector3::BACK();
		if (IsKeyDown(GLFW_KEY_A)) cam_tr += Vector3::LEFT();
		if (IsKeyDown(GLFW_KEY_D)) cam_tr += Vector3::RIGHT();
		camera->Translate(cam_tr * moveSpeed * dtf);
	}

	// Camera look-around
	{
		int mouseMode = glfwGetInputMode(window, GLFW_CURSOR);
		if (IsMouseInsideWindow() && IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
			if (mouseMode != GLFW_CURSOR_DISABLED) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			} else {
				camRotation += cursorSpeed * 0.05f;
				camRotation.y = Clamp(camRotation.y, -90.0f, 90.0f);
				camera->SetRotation(Quaternion(camRotation.y, camRotation.x, 0.0f));
			}

		} else if (mouseMode != GLFW_CURSOR_NORMAL) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
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

	Render(dt);
}

void Application::PostUpdate(double dt)
{
	cursorSpeed = Vector2::ZERO();
}

void Application::FixedUpdate(double dt)
{
}

void Application::Render(double dt)
{
	const IntRect viewRect {IntVector2::ZERO(), colorBuffer->Size2D()};

	// Collect geometries and lights in frustum.
	// Also set debug renderer to use the correct camera view.
	renderer->PrepareView(scene.get(), camera.get(), true, useOcclusion, (float)dt);
	debugRenderer->SetView(camera.get());

	// Now render the scene, starting with shadowmaps and opaque geometries
	renderer->RenderShadowMaps();
	graphics->SetViewport(viewRect);

	// The default opaque shaders can write both color (first RT) and view-space normals (second RT).
	hdrFbo->Bind();
	renderer->RenderOpaque();
	renderer->RenderAlpha();

	// Resolve MSAA
	if (multiSample > 1) {
		graphics->Blit(colorFbo[0].get(), viewRect, colorFbo[1].get(), viewRect, true, false, FILTER_BILINEAR);
		graphics->Blit(normalFbo[0].get(), viewRect, normalFbo[1].get(), viewRect, true, false, FILTER_BILINEAR);
		graphics->Blit(depthFbo[0].get(), viewRect, depthFbo[1].get(), viewRect, false, true, FILTER_POINT);
	}

	Texture* color = colorBuffer.get(); // Resolved HDR color texture
	//Texture* normal = normalBuffer[0].get(); // Resolved normal texture
	//Texture* depth = depthStencilBuffer[0].get(); // Resolved Depth texture

	// SSAO
	if (aoRenderer) {
		TURSO3D_GL_MARKER("SAO");
		aoRenderer->Render(camera.get(), normalBuffer.get(), depthBuffer.get(), ldrFbo.get(), viewRect);
	}

	// HDR Bloom
	if (bloomRenderer) {
		TURSO3D_GL_MARKER("Bloom");
		bloomRenderer->Render(color, 0.02f);
		color = bloomRenderer->GetTexture();
	}

	// Tonemap
	if (tonemapRenderer) {
		TURSO3D_GL_MARKER("Tonemap");
		ldrFbo->Bind();
		tonemapRenderer->Render(color);
	}

	// Optional render of debug geometry
	if (renderDebug) {
		renderer->RenderDebug(debugRenderer.get());
		debugRenderer->Render();
	}

	graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);

	// Blur scene for UI transparent backgrounds
	{
		TURSO3D_GL_MARKER("Scene Blur");
		blurRenderer->Downsample(ldrBuffer.get());
		blurRenderer->Upsample();
		graphics->SetViewport(viewRect);
	}

	// Compose UI
	if (uiManager) {
		FrameBuffer::Unbind();
		uiManager->Compose(ldrBuffer.get(), blurRenderer->GetTexture());
	}

	graphics->Present();
}
