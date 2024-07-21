#include <Turso3D/Renderer/Renderer.h>
#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/IndexBuffer.h>
#include <Turso3D/Graphics/RenderBuffer.h>
#include <Turso3D/Graphics/Shader.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Graphics/UniformBuffer.h>
#include <Turso3D/Graphics/VertexBuffer.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Renderer/AnimatedModel.h>
#include <Turso3D/Renderer/Animation.h>
#include <Turso3D/Renderer/Batch.h>
#include <Turso3D/Renderer/Camera.h>
#include <Turso3D/Renderer/DebugRenderer.h>
#include <Turso3D/Renderer/Light.h>
#include <Turso3D/Renderer/LightEnvironment.h>
#include <Turso3D/Renderer/Material.h>
#include <Turso3D/Renderer/Model.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Resource/ResourceCache.h>
#include <Turso3D/Scene/Scene.h>
#include <algorithm>
#include <cstring>

namespace
{
	using namespace Turso3D;

	constexpr size_t INSTANCE_BUFFER_CAPACITY = 64000;
	constexpr size_t INITIAL_INSTANCE_CAPACITY = 2000;
	constexpr size_t DRAWABLES_PER_BATCH_TASK = 128;
	constexpr size_t NUM_BOX_INDICES = 36;
	constexpr float OCCLUSION_MARGIN = 0.1f;

	static inline bool CompareDrawableDistances(Drawable* lhs, Drawable* rhs)
	{
		return lhs->Distance() < rhs->Distance();
	}
}

namespace Turso3D
{
	// Task for collecting octants.
	struct CollectOctantsTask : public MemberFunctionTask<Renderer>
	{
		// Construct.
		CollectOctantsTask(Renderer* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Renderer>(object_, function_)
		{
		}

		// Starting point octant.
		Octant* startOctant;
		// Result structure index.
		size_t resultIdx;
	};

	// Task for collecting geometry batches from octants.
	struct CollectBatchesTask : public MemberFunctionTask<Renderer>
	{
		// Construct.
		CollectBatchesTask(Renderer* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Renderer>(object_, function_)
		{
		}

		// Octant list with plane masks.
		std::vector<std::pair<Octant*, unsigned char >> octants;
	};

	// Task for collecting shadowcasters of a specific light.
	struct CollectShadowCastersTask : public MemberFunctionTask<Renderer>
	{
		// Construct.
		CollectShadowCastersTask(Renderer* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Renderer>(object_, function_)
		{
		}

		// Light.
		LightDrawable* light;
	};

	// Task for collecting shadow batches of a specific shadow view.
	struct CollectShadowBatchesTask : public MemberFunctionTask<Renderer>
	{
		// Construct.
		CollectShadowBatchesTask(Renderer* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Renderer>(object_, function_)
		{
		}

		// Shadow map index.
		size_t shadowMapIdx;
		// Shadow view index within shadow map.
		size_t viewIdx;
	};

	// Task for culling lights to a specific Z-slice of the frustum grid.
	struct CullLightsTask : public MemberFunctionTask<Renderer>
	{
		// Construct.
		CullLightsTask(Renderer* object_, MemberWorkFunctionPtr function_) :
			MemberFunctionTask<Renderer>(object_, function_)
		{
		}

		// Z-slice.
		size_t z;
	};

	// ==========================================================================================
	void ThreadOctantResult::Clear()
	{
		drawableAcc = 0;
		taskOctantIdx = 0;
		batchTaskIdx = 0;
		lights.clear();
		octants.clear();
		occlusionQueries.clear();
	}

	void ThreadBatchResult::Clear()
	{
		minZ = M_MAX_FLOAT;
		maxZ = 0.0f;
		geometryBounds.Undefine();
		opaqueBatches.clear();
		alphaBatches.clear();
	}

	// ==========================================================================================
	ShadowMap::ShadowMap() :
		freeQueueIdx(0),
		freeCasterListIdx(0)
	{
		// Construct texture but do not define its size yet
		texture = std::make_unique<Texture>();
		fbo = std::make_unique<FrameBuffer>();
	}

	void ShadowMap::Clear()
	{
		freeQueueIdx = 0;
		freeCasterListIdx = 0;

		const IntVector3& tex_size = texture->Size();
		allocator.Reset(tex_size.x, tex_size.y, 0, 0, false);
		shadowViews.clear();
		instanceTransforms.clear();

		for (size_t i = 0; i < shadowBatches.size(); ++i) {
			shadowBatches[i].Clear();
		}
		for (size_t i = 0; i < shadowCasters.size(); ++i) {
			shadowCasters[i].clear();
		}
	}

	// ==========================================================================================
	Renderer::Renderer(WorkQueue* workQueue) :
		workQueue(workQueue),
		frameNumber(0),
		clusterFrustumsDirty(true),
		depthBiasMul(1.0f),
		slopeScaleBiasMul(1.0f)
	{
		assert(Graphics::IsInitialized());

		instanceVertexBuffer = std::make_unique<VertexBuffer>();
		const VertexElement elements[] = {
			{ELEM_VECTOR4, ATTR_WORLDINSTANCE_M0},
			{ELEM_VECTOR4, ATTR_WORLDINSTANCE_M1},
			{ELEM_VECTOR4, ATTR_WORLDINSTANCE_M2}
		};
		instanceVertexBuffer->Define(USAGE_DYNAMIC, INSTANCE_BUFFER_CAPACITY, elements, 3);
		instanceVertexBufferOffset = 0;
		instanceTransforms.reserve(INITIAL_INSTANCE_CAPACITY);

		clusterTexture = std::make_unique<Texture>();
		clusterTexture->Define(TARGET_3D, IntVector3 {NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z}, FORMAT_RGBA32_UINT_PACK32);
		clusterTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		clusterCullData = std::make_unique<ClusterCullData[]>(NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z);
		clusterData = std::make_unique<uint8_t[]>(MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z);
		lightData = std::make_unique<LightData[]>(MAX_LIGHTS + 1);

		perViewDataBuffer = std::make_unique<UniformBuffer>();
		perViewDataBuffer->Define(USAGE_DYNAMIC, sizeof(PerViewUniforms));

		lightDataBuffer = std::make_unique<UniformBuffer>();
		lightDataBuffer->Define(USAGE_DYNAMIC, (MAX_LIGHTS + 1) * sizeof(LightData));

		octantResults = std::make_unique<ThreadOctantResult[]>(NUM_OCTANT_TASKS);
		batchResults = std::make_unique<ThreadBatchResult[]>(workQueue->NumThreads());

		for (size_t i = 0; i < NUM_OCTANTS + 1; ++i) {
			collectOctantsTasks[i] = std::make_unique<CollectOctantsTask>(this, &Renderer::CollectOctantsWork);
			collectOctantsTasks[i]->resultIdx = i;
		}

		for (size_t z = 0; z < NUM_CLUSTER_Z; ++z) {
			cullLightsTasks[z] = std::make_unique<CullLightsTask>(this, &Renderer::CullLightsToFrustumWork);
			cullLightsTasks[z]->z = z;
		}

		processLightsTask = std::make_unique<MemberFunctionTask<Renderer>>(this, &Renderer::ProcessLightsWork);
		batchesReadyTask = std::make_unique<MemberFunctionTask<Renderer>>(this, &Renderer::BatchesReadyWork);
		processShadowCastersTask = std::make_unique<MemberFunctionTask<Renderer>>(this, &Renderer::ProcessShadowCastersWork);

		DefineBoundingBoxGeometry();
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format)
	{
		if (!shadowMaps) {
			shadowMaps = std::make_unique<ShadowMap[]>(NUM_SHADOW_MAPS);
		}

		for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i) {
			ShadowMap& shadowMap = shadowMaps[i];

			shadowMap.texture->Define(TARGET_2D, i == 0 ? IntVector3 {dirLightSize * 2, dirLightSize, 1} : IntVector3 {lightAtlasSize, lightAtlasSize, 1}, format);
			shadowMap.texture->DefineSampler(COMPARE_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP, 1);
			shadowMap.fbo->Define(nullptr, shadowMap.texture.get());
		}

		if (!staticObjectShadowBuffer) {
			staticObjectShadowBuffer = std::make_unique<RenderBuffer>();
		}
		if (!staticObjectShadowFbo) {
			staticObjectShadowFbo = std::make_unique<FrameBuffer>();
		}

		staticObjectShadowBuffer->Define(IntVector2 {lightAtlasSize, lightAtlasSize}, format);
		staticObjectShadowFbo->Define(nullptr, staticObjectShadowBuffer.get());

		DefineFaceSelectionTextures();

		shadowMapsDirty = true;
	}

	void Renderer::SetShadowDepthBiasMul(float depthBiasMul_, float slopeScaleBiasMul_)
	{
		depthBiasMul = depthBiasMul_;
		slopeScaleBiasMul = slopeScaleBiasMul_;

		// Need to rerender all shadow maps with changed bias
		shadowMapsDirty = true;
	}

	void Renderer::PrepareView(Scene* scene_, Camera* camera_, bool drawShadows_, bool useOcclusion_, float lastFrameTime_)
	{
		if (!scene_ || !camera_) {
			return;
		}

		scene = scene_;
		camera = camera_;
		octree = scene->GetOctree();
		lightEnvironment = scene->GetEnvironmentLighting();
		if (!octree) {
			return;
		}

		// Framenumber is never 0
		++frameNumber;
		if (!frameNumber) {
			++frameNumber;
		}

		drawShadows = shadowMaps ? drawShadows_ : false;
		useOcclusion = useOcclusion_;
		frustum = camera->WorldFrustum();
		viewMask = camera->ViewMask();

		// Clear results from last frame
		dirLight = nullptr;
		lastCamera = nullptr;
		rootLevelOctants.clear();
		opaqueBatches.Clear();
		alphaBatches.Clear();
		lights.clear();

		instanceVertexBufferOffset += instanceTransforms.size();
		instanceTransforms.clear();

		minZ = M_MAX_FLOAT;
		maxZ = 0.0f;
		geometryBounds.Undefine();

		// Stagger for occlusion queries based on last frametime
		lastFrameTime = lastFrameTime_; //graphics->LastFrameTime();

		for (size_t i = 0; i < NUM_OCTANT_TASKS; ++i) {
			octantResults[i].Clear();
		}
		for (size_t i = 0; i < workQueue->NumThreads(); ++i) {
			batchResults[i].Clear();
		}

		if (shadowMaps) {
			for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i) {
				shadowMaps[i].Clear();
			}
		}

		// Process moved / animated objects' octree reinsertions
		octree->Update(frameNumber);

		// Precalculate SAT test parameters for accurate frustum test (verify what octants to occlusion query)
		if (useOcclusion) {
			frustumSATData.Calculate(frustum);
		}

		// Check arrived occlusion query results while octree update goes on, then finish octree update
		CheckOcclusionQueries();
		octree->FinishUpdate();

		// Find the starting points for octree traversal. Include the root if it contains drawables that didn't fit elsewhere
		Octant* rootOctant = octree->Root();
		if (rootOctant->Drawables().size()) {
			rootLevelOctants.push_back(rootOctant);
		}

		for (size_t i = 0; i < NUM_OCTANTS; ++i) {
			if (rootOctant->Child(i)) {
				rootLevelOctants.push_back(rootOctant->Child(i));
			}
		}

		// If no root level octants, must early-out the view preparation; there is nothing to render and task dependencies would not complete
		if (rootLevelOctants.empty()) {
			return;
		}

		// Enable threaded update during geometry / light gathering in case nodes' OnPrepareRender() causes further reinsertion queuing
		octree->SetThreadedUpdate(workQueue->NumThreads() > 1);

		// Keep track of both batch + octant task progress before main batches can be sorted (batch tasks will add to the counter when queued)
		numPendingBatchTasks.store((int)rootLevelOctants.size());
		numPendingShadowViews[0].store(0);
		numPendingShadowViews[1].store(0);

		// Ensure shadowcaster processing doesn't happen before lights have been found and processed, and geometry bounds are known
		// Note: this task is also needed without shadows, as it initiates light grid culling
		workQueue->AddDependency(processShadowCastersTask.get(), processLightsTask.get());
		workQueue->AddDependency(processShadowCastersTask.get(), batchesReadyTask.get());

		// Find octants in view and their plane masks for node frustum culling. At the same time, find lights and process them
		// When octant collection tasks complete, they queue tasks for collecting batches from those octants.
		for (size_t i = 0; i < rootLevelOctants.size(); ++i) {
			collectOctantsTasks[i]->startOctant = rootLevelOctants[i];
			workQueue->AddDependency(processLightsTask.get(), collectOctantsTasks[i].get());
		}
		workQueue->QueueTasks(rootLevelOctants.size(), reinterpret_cast<Task**>(&collectOctantsTasks[0]));

		// Execute tasks until can sort the main batches. Perform that in the main thread to potentially run faster
		while (numPendingBatchTasks.load() > 0) {
			workQueue->TryComplete();
		}

		SortMainBatches();

		// Finish remaining view preparation tasks (shadowcaster batches, light culling to frustum grid)
		workQueue->Complete();

		// No more threaded reinsertion will take place
		octree->SetThreadedUpdate(false);
	}

	void Renderer::RenderShadowMaps()
	{
		if (!shadowMaps) {
			return;
		}

		// Unbind shadow textures before rendering to
		Graphics::BindTexture(TU_DIRLIGHTSHADOW, nullptr);
		Graphics::BindTexture(TU_SHADOWATLAS, nullptr);

		for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i) {
			ShadowMap& shadowMap = shadowMaps[i];
			if (shadowMap.shadowViews.empty()) {
				continue;
			}

			UpdateInstanceTransforms(shadowMap.instanceTransforms);

			Graphics::BindFramebuffer(shadowMap.fbo.get(), nullptr);

			// First render static objects for those shadowmaps that need to store static objects. Do all of them to avoid FBO changes
			for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j) {
				ShadowView* view = shadowMap.shadowViews[j];
				LightDrawable* light = view->light;

				if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC) {
					Graphics::Clear(false, true, view->viewport);

					BatchQueue& batchQueue = shadowMap.shadowBatches[view->staticQueueIdx];
					if (batchQueue.HasBatches()) {
						Graphics::SetViewport(view->viewport);
						Graphics::SetDepthBias(light->DepthBias() * depthBiasMul, light->SlopeScaleBias() * slopeScaleBiasMul);
						RenderBatches(view->shadowCamera.get(), batchQueue);
					}
				}
			}

			// Now do the shadowmap -> static shadowmap storage blits as necessary
			for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j) {
				ShadowView* view = shadowMap.shadowViews[j];

				if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC) {
					Graphics::Blit(staticObjectShadowFbo.get(), view->viewport, shadowMap.fbo.get(), view->viewport, false, true, FILTER_POINT);
				}
			}

			// Rebind shadowmap
			Graphics::BindFramebuffer(shadowMap.fbo.get(), nullptr);

			// First do all the clears or static shadowmap -> shadowmap blits
			for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j) {
				ShadowView* view = shadowMap.shadowViews[j];

				if (view->renderMode == RENDER_DYNAMIC_LIGHT) {
					Graphics::Clear(false, true, view->viewport);
				} else if (view->renderMode == RENDER_STATIC_LIGHT_RESTORE_STATIC) {
					Graphics::Blit(shadowMap.fbo.get(), view->viewport, staticObjectShadowFbo.get(), view->viewport, false, true, FILTER_POINT);
				}
			}

			// Finally render the dynamic objects
			for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j) {
				ShadowView* view = shadowMap.shadowViews[j];
				LightDrawable* light = view->light;

				if (view->renderMode != RENDER_STATIC_LIGHT_CACHED) {
					BatchQueue& batchQueue = shadowMap.shadowBatches[view->dynamicQueueIdx];
					if (batchQueue.HasBatches()) {
						Graphics::SetViewport(view->viewport);
						Graphics::SetDepthBias(light->DepthBias() * depthBiasMul, light->SlopeScaleBias() * slopeScaleBiasMul);
						RenderBatches(view->shadowCamera.get(), batchQueue);
					}
				}
			}
		}

		Graphics::SetDepthBias(0.0f, 0.0f);
	}

	void Renderer::RenderOpaque(bool clear)
	{
		// Update main batches' instance transforms & light data
		UpdateInstanceTransforms(instanceTransforms);
		UpdateLightData();

		if (shadowMaps) {
			Graphics::BindTexture(TU_DIRLIGHTSHADOW, shadowMaps[0].texture.get());
			Graphics::BindTexture(TU_SHADOWATLAS, shadowMaps[1].texture.get());
			Graphics::BindTexture(TU_FACESELECTION, faceSelectionTexture.get());
		}

		Graphics::BindTexture(TU_LIGHTCLUSTERDATA, clusterTexture.get());
		Graphics::BindUniformBuffer(UB_LIGHTDATA, lightDataBuffer.get());

		if (clear) {
			Graphics::Clear(true, true, IntRect::ZERO(), lightEnvironment->FogColor());
		}

		if (Texture* tex = lightEnvironment->GetIEMTexture(); tex) {
			Graphics::BindTexture(TU_IBL_IEM, tex);
		}
		if (Texture* tex = lightEnvironment->GetPMREMTexture(); tex) {
			Graphics::BindTexture(TU_IBL_PMREM, tex);
		}
		if (Texture* tex = lightEnvironment->GetBRDFTexture(); tex) {
			Graphics::BindTexture(TU_IBL_BRDFLUT, tex);
		}

		RenderBatches(camera, opaqueBatches);

		// Render occlusion now after opaques
		if (useOcclusion) {
			RenderOcclusionQueries();
		}
	}

	void Renderer::RenderAlpha()
	{
		if (shadowMaps) {
			Graphics::BindTexture(TU_DIRLIGHTSHADOW, shadowMaps[0].texture.get());
			Graphics::BindTexture(TU_SHADOWATLAS, shadowMaps[1].texture.get());
			Graphics::BindTexture(TU_FACESELECTION, faceSelectionTexture.get());
		}

		Graphics::BindTexture(TU_LIGHTCLUSTERDATA, clusterTexture.get());
		Graphics::BindUniformBuffer(UB_LIGHTDATA, lightDataBuffer.get());

		if (Texture* tex = lightEnvironment->GetIEMTexture(); tex) {
			Graphics::BindTexture(TU_IBL_IEM, tex);
		}
		if (Texture* tex = lightEnvironment->GetPMREMTexture(); tex) {
			Graphics::BindTexture(TU_IBL_PMREM, tex);
		}
		if (Texture* tex = lightEnvironment->GetBRDFTexture(); tex) {
			Graphics::BindTexture(TU_IBL_BRDFLUT, tex);
		}

		RenderBatches(camera, alphaBatches);
	}

	void Renderer::RenderDebug(DebugRenderer* debugRenderer)
	{
		if (!debugRenderer) {
			return;
		}

		for (size_t i = 0; i < lights.size(); ++i) {
			LightDrawable* light = lights[i];
			light->OnRenderDebug(debugRenderer);
		}

		for (size_t i = 0; i < rootLevelOctants.size(); ++i) {
			const ThreadOctantResult& result = octantResults[i];

			for (size_t j = 0; j < result.octants.size(); ++j) {
				Octant* octant = result.octants[j].first;
				octant->OnRenderDebug(debugRenderer);

				const std::vector<Drawable*>& drawables = octant->Drawables();
				for (size_t k = 0; k < drawables.size(); ++k) {
					Drawable* drawable = drawables[k];
					if (drawable->TestFlag(Drawable::FLAG_GEOMETRY) && drawable->LastFrameNumber() == frameNumber) {
						drawable->OnRenderDebug(debugRenderer);
					}
				}
			}
		}
	}

	Texture* Renderer::ShadowMapTexture(size_t index) const
	{
		return (shadowMaps && index < NUM_SHADOW_MAPS) ? shadowMaps[index].texture.get() : nullptr;
	}

	void Renderer::CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, unsigned char planeMask)
	{
		const BoundingBox& octantBox = octant->CullingBox();

		if (planeMask) {
			// If not already inside all frustum planes, do frustum test and terminate if completely outside
			planeMask = frustum.IsInsideMasked(octantBox, planeMask);
			if (planeMask == 0xff) {
				// If octant becomes frustum culled, reset its visibility for when it comes back to view, including its children
				if (useOcclusion && octant->Visibility() != VIS_OUTSIDE_FRUSTUM) {
					octant->SetVisibility(VIS_OUTSIDE_FRUSTUM, true);
				}
				return;
			}
		}

		// Process occlusion now before going further
		if (useOcclusion) {
			// If was previously outside frustum, reset to visible-unknown
			if (octant->Visibility() == VIS_OUTSIDE_FRUSTUM) {
				octant->SetVisibility(VIS_VISIBLE_UNKNOWN, false);
			}

			switch (octant->Visibility()) {
				// If octant is occluded, issue query if not pending, and do not process further this frame
				case VIS_OCCLUDED:
					AddOcclusionQuery(octant, result, planeMask);
					return;

					// If octant was occluded previously, but its parent came into view, issue tests along the hierarchy but do not render on this frame
				case VIS_OCCLUDED_UNKNOWN:
					AddOcclusionQuery(octant, result, planeMask);
					if (octant != octree->Root() && octant->HasChildren()) {
						for (size_t i = 0; i < NUM_OCTANTS; ++i) {
							if (octant->Child(i)) {
								CollectOctantsAndLights(octant->Child(i), result, planeMask);
							}
						}
					}
					return;

					// If octant has unknown visibility, issue query if not pending, but collect child octants and drawables
				case VIS_VISIBLE_UNKNOWN:
					AddOcclusionQuery(octant, result, planeMask);
					break;

					// If the octant's parent is already visible too, only test the octant if it is a "leaf octant" with drawables
					// Note: visible octants will also add a time-based staggering to reduce queries
				case VIS_VISIBLE:
					Octant* parent = octant->Parent();
					if (octant->Drawables().size() > 0 || (parent && parent->Visibility() != VIS_VISIBLE)) {
						AddOcclusionQuery(octant, result, planeMask);
					}
					break;
			}
		} else {
			// When occlusion not in use, reset all traversed octants to visible-unknown
			octant->SetVisibility(VIS_VISIBLE_UNKNOWN, false);
		}

		const std::vector<Drawable*>& drawables = octant->Drawables();
		for (size_t i = 0; i < drawables.size(); ++i) {
			Drawable* drawable = drawables[i];

			if (drawable->TestFlag(Drawable::FLAG_LIGHT)) {
				const BoundingBox& lightBox = drawable->WorldBoundingBox();
				if ((drawable->ViewMask() & viewMask) && (!planeMask || frustum.IsInsideMaskedFast(lightBox, planeMask)) && drawable->OnPrepareRender(frameNumber, camera)) {
					result.lights.push_back(static_cast<LightDrawable*>(drawable));
				}
			} else {
				// Lights are sorted first in octants, so break when first geometry encountered. Store the octant for batch collecting
				result.octants.push_back(std::make_pair(octant, planeMask));
				result.drawableAcc += drawables.size() - i;
				break;
			}
		}

		// Setup and queue batches collection task if over the drawable limit now
		if (result.drawableAcc >= DRAWABLES_PER_BATCH_TASK) {
			if (result.collectBatchesTasks.size() <= result.batchTaskIdx) {
				result.collectBatchesTasks.push_back(std::make_unique<CollectBatchesTask>(this, &Renderer::CollectBatchesWork));
			}

			CollectBatchesTask* batchTask = result.collectBatchesTasks[result.batchTaskIdx].get();
			batchTask->octants.clear();
			batchTask->octants.insert(batchTask->octants.end(), result.octants.begin() + result.taskOctantIdx, result.octants.end());
			numPendingBatchTasks.fetch_add(1);
			workQueue->QueueTask(batchTask);

			result.drawableAcc = 0;
			result.taskOctantIdx = result.octants.size();
			++result.batchTaskIdx;
		}

		// Root octant is handled separately. Otherwise recurse into child octants
		if (octant != octree->Root() && octant->HasChildren()) {
			for (size_t i = 0; i < NUM_OCTANTS; ++i) {
				if (octant->Child(i)) {
					CollectOctantsAndLights(octant->Child(i), result, planeMask);
				}
			}
		}
	}

	void Renderer::AddOcclusionQuery(Octant* octant, ThreadOctantResult& result, unsigned char planeMask)
	{
		// No-op if previous query still ongoing. Also If the octant intersects the frustum, verify with SAT test that it actually covers some screen area
		// Otherwise the occlusion test will produce a false negative
		if (octant->CheckNewOcclusionQuery(lastFrameTime) && (!planeMask || frustum.IsInsideSAT(octant->CullingBox(), frustumSATData))) {
			result.occlusionQueries.push_back(octant);
		}
	}

	bool Renderer::AllocateShadowMap(LightDrawable* light)
	{
		size_t index = light->GetLightType() == LIGHT_DIRECTIONAL ? 0 : 1;
		ShadowMap& shadowMap = shadowMaps[index];

		IntVector2 request = light->TotalShadowMapSize();

		// If light already has its preferred shadow rect from the previous frame, try to reallocate it for shadow map caching
		IntRect oldRect = light->ShadowRect();
		if (request.x == oldRect.Width() && request.y == oldRect.Height()) {
			if (shadowMap.allocator.AllocateSpecific(oldRect)) {
				light->SetShadowMap(shadowMaps[index].texture.get(), light->ShadowRect());
				return true;
			}
		}

		IntRect shadowRect;
		size_t retries = 3;

		while (retries--) {
			int x, y;
			if (shadowMap.allocator.Allocate(request.x, request.y, x, y)) {
				light->SetShadowMap(shadowMaps[index].texture.get(), IntRect(x, y, x + request.x, y + request.y));
				return true;
			}

			request.x /= 2;
			request.y /= 2;
		}

		// No room in atlas
		light->SetShadowMap(nullptr);
		return false;
	}

	void Renderer::SortMainBatches()
	{
		// Shadowcaster processing needs accurate scene min / max Z results, combine them from per-thread data
		for (size_t i = 0; i < workQueue->NumThreads(); ++i) {
			ThreadBatchResult& res = batchResults[i];
			minZ = std::min(minZ, res.minZ);
			maxZ = std::max(maxZ, res.maxZ);
			if (res.geometryBounds.IsDefined()) {
				geometryBounds.Merge(res.geometryBounds);
			}
		}

		minZ = std::max(minZ, camera->NearClip());

		// Signal that shadowcaster processing is OK to happen
		workQueue->QueueTask(batchesReadyTask.get());

		// Join per-thread collected batches and sort
		for (size_t i = 0; i < workQueue->NumThreads(); ++i) {
			ThreadBatchResult& res = batchResults[i];
			if (res.opaqueBatches.size()) {
				opaqueBatches.batches.insert(opaqueBatches.batches.end(), res.opaqueBatches.begin(), res.opaqueBatches.end());
			}
			if (res.alphaBatches.size()) {
				alphaBatches.batches.insert(alphaBatches.batches.end(), res.alphaBatches.begin(), res.alphaBatches.end());
			}
		}

		opaqueBatches.Sort(instanceTransforms, BatchSortMode::StateDistance, true);
		alphaBatches.Sort(instanceTransforms, BatchSortMode::Distance, true);
	}

	void Renderer::SortShadowBatches(ShadowMap& shadowMap)
	{
		for (size_t i = 0; i < shadowMap.shadowViews.size(); ++i) {
			ShadowView& view = *shadowMap.shadowViews[i];
			LightDrawable* light = view.light;

			// Check if view was discarded during shadowcaster collecting
			if (!light) {
				continue;
			}

			BatchQueue* destStatic = (view.renderMode == RENDER_STATIC_LIGHT_STORE_STATIC) ? &shadowMap.shadowBatches[view.staticQueueIdx] : nullptr;
			BatchQueue* destDynamic = &shadowMap.shadowBatches[view.dynamicQueueIdx];

			if (destStatic && destStatic->HasBatches()) {
				destStatic->Sort(shadowMap.instanceTransforms, BatchSortMode::State, true);
			}

			if (destDynamic->HasBatches()) {
				destDynamic->Sort(shadowMap.instanceTransforms, BatchSortMode::State, true);
			}
		}
	}

	void Renderer::UpdateInstanceTransforms(const std::vector<Matrix3x4>& transforms)
	{
		if (transforms.size()) {
			bool discard = instanceVertexBufferOffset + transforms.size() > instanceVertexBuffer->NumVertices();
			size_t offset = discard ? 0 : instanceVertexBufferOffset;
			instanceVertexBuffer->SetData(offset, transforms.size(), transforms.data(), discard);
			if (discard) {
				instanceVertexBufferOffset = 0;
			}
		}
	}

	void Renderer::UpdateLightData()
	{
		ImageLevel clusterLevel {
			clusterData.get(),
			0,
			IntBox {0, 0, 0, NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z},
			0,
			0
		};
		clusterTexture->SetData(clusterLevel);
		lightDataBuffer->SetData(0, (lights.size() + 1) * sizeof(LightData), lightData.get());
	}

	void Renderer::RenderBatches(Camera* camera_, const BatchQueue& queue)
	{
		lastMaterial = nullptr;
		lastPass = nullptr;

		if (camera_ != lastCamera) {
			float nearClip = camera->NearClip();
			float farClip = camera->FarClip();

			perViewData.projectionMatrix = camera_->ProjectionMatrix();
			perViewData.viewMatrix = camera_->ViewMatrix();
			perViewData.viewProjMatrix = perViewData.projectionMatrix * perViewData.viewMatrix;
			perViewData.depthParameters = Vector4(nearClip, farClip, camera_->IsOrthographic() ? 0.5f : 0.0f, camera_->IsOrthographic() ? 0.5f : 1.0f / farClip);
			perViewData.cameraPosition = Vector4(camera_->WorldPosition(), 1.0f);

			size_t dataSize = sizeof(PerViewUniforms);

			// Set global lighting settings if is the main view
			if (camera_ == camera) {
				perViewData.ambientColor = lightEnvironment->AmbientColor();
				if (Texture* tex = lightEnvironment->GetPMREMTexture(); tex && tex->NumLevels()) {
					float maxMip = std::max(static_cast<float>(tex->NumLevels()) - 1.0f, 0.0f);
					perViewData.iblParameters = Vector4 {maxMip, 0.0f, 0.0f, 0.0f};
				}
			}

			// Set directional light data if exists and is the main view
			if (!dirLight || camera_ != camera) {
				perViewData.dirLightDirection = Vector4::ZERO();
				perViewData.dirLightColor = Color::BLACK();
				perViewData.dirLightShadowParameters = Vector4::ONE();
				dataSize -= 2 * sizeof(Matrix4); // Leave out shadow matrices
			} else {
				perViewData.dirLightDirection = Vector4(-dirLight->WorldDirection(), 0.0f);
				perViewData.dirLightColor = dirLight->GetColor();

				if (dirLight->ShadowMap()) {
					Vector2 cascadeSplits = dirLight->ShadowCascadeSplits();
					float firstSplit = cascadeSplits.x / farClip;
					float secondSplit = cascadeSplits.y / farClip;

					perViewData.dirLightShadowSplits = Vector4(firstSplit, secondSplit, dirLight->ShadowFadeStart() * secondSplit, 1.0f / (secondSplit - dirLight->ShadowFadeStart() * secondSplit));
					perViewData.dirLightShadowParameters = dirLight->ShadowParameters();
					if (dirLight->ShadowViews().size() >= 2) {
						perViewData.dirLightShadowMatrices[0] = dirLight->ShadowViews()[0].shadowMatrix;
						perViewData.dirLightShadowMatrices[1] = dirLight->ShadowViews()[1].shadowMatrix;
					}
				} else {
					perViewData.dirLightShadowParameters = Vector4::ONE();
					dataSize -= 2 * sizeof(Matrix4); // Leave out shadow matrices
				}
			}

			perViewDataBuffer->SetData(0, dataSize, &perViewData);

			lastCamera = camera_;
		}

		Graphics::BindUniformBuffer(UB_PERVIEWDATA, perViewDataBuffer.get());

		const std::vector<Batch>& batches = queue.batches;
		for (size_t i = 0; i < batches.size(); ++i) {
			const Batch& batch = batches[i];

			// Select permutation for pass program
			ShaderProgram* program;
			{
				Pass::GeometryPermutation gp = Pass::GeometryPermutation::None;
				if (batch.type == BatchType::Instanced) {
					gp = Pass::GeometryPermutation::Instanced;
				} else if (batch.drawableFlags & Drawable::FLAG_SKINNED_GEOMETRY) {
					gp = Pass::GeometryPermutation::Skinned;
				}

				Pass::LightMaskPermutation lmp = Pass::LightMaskPermutation::Disabled;
				if (batch.lightMask) {
					lmp = Pass::LightMaskPermutation::Enabled;
				}

				program = batch.pass->GetShaderProgram(gp, lmp);
			}
			if (!program) {
				continue;
			}

			Graphics::BindProgram(program);

			Material* material = batch.pass->Parent();
			if (batch.pass != lastPass) {
				if (material != lastMaterial) {
					for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i) {
						Texture* texture = material->GetTexture(i).get();
						if (texture) {
							Graphics::BindTexture(i, texture);
						}
					}

					UniformBuffer* materialUniforms = material->GetUniformBuffer();
					if (materialUniforms) {
						Graphics::BindUniformBuffer(UB_MATERIALDATA, materialUniforms);
					}
					lastMaterial = material;
				}

				CullMode cullMode = material->GetCullMode();
				if (camera_->UseReverseCulling()) {
					if (cullMode == CULL_BACK) {
						cullMode = CULL_FRONT;
					} else if (cullMode == CULL_FRONT) {
						cullMode = CULL_BACK;
					}
				}

				Graphics::SetRenderState(batch.pass->GetBlendMode(), cullMode, batch.pass->GetDepthTest(), batch.pass->GetColorWrite(), batch.pass->GetDepthWrite());
				lastPass = batch.pass;
			}

			Geometry* geometry = batch.geometry;
			bool instanced = (batch.type == BatchType::Instanced);

			// Bind vertex buffers
			const VertexBufferBinding bindings[] = {
				{geometry->vertexBuffer.get()},
				{instanceVertexBuffer.get(), instanceVertexBufferOffset + batch.instanceStart, 1, instanced}
			};
			Graphics::BindVertexBuffers(bindings, 2);

			// Bind index buffer
			IndexBuffer* ib = geometry->indexBuffer.get();
			if (ib) {
				Graphics::BindIndexBuffer(ib);
			}

			if (batch.lightMask) {
				program->SetUniform(U_LIGHTMASK, batch.lightMask);
			}

			if (instanced) {
				if (ib) {
					Graphics::DrawIndexedInstanced(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount, batch.instanceCount);
				} else {
					Graphics::DrawInstanced(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount, batch.instanceCount);
				}
				i += batch.instanceCount - 1;

			} else {
				if (batch.type == BatchType::Static) {
					program->SetUniform(U_WORLDMATRIX, *batch.worldTransform);
				} else {
					batch.drawable->OnRender(program, batch.geomIndex);
				}

				if (ib) {
					Graphics::DrawIndexed(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount);
				} else {
					Graphics::Draw(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount);
				}
			}
		}
	}

	void Renderer::CheckOcclusionQueries()
	{
		constexpr float target_rate = 1.0f / 60.0f;

		occlusionQueryResults.clear();
		Graphics::CheckOcclusionQueryResults(occlusionQueryResults, lastFrameTime < target_rate);

		for (size_t i = 0; i < occlusionQueryResults.size(); ++i) {
			OcclusionQueryResult& result = occlusionQueryResults[i];

			Octant* octant = static_cast<Octant*>(result.object);
			octant->OnOcclusionQueryResult(result.visible);
		}
	}

	void Renderer::RenderOcclusionQueries()
	{
		if (!boundingBoxShaderProgram) {
			return;
		}
		Graphics::BindProgram(boundingBoxShaderProgram.get());

		Matrix3x4 boxMatrix {Matrix3x4::IDENTITY()};
		float nearClip = camera->NearClip();

		// Use camera's motion since last frame to enlarge the bounding boxes.
		// Use multiplied movement speed to account for latency in query results
		Vector3 cameraPosition = camera->WorldPosition();
		Vector3 cameraMove = cameraPosition - previousCameraPosition;
		Vector3 enlargement = (OCCLUSION_MARGIN + 4.0f * cameraMove.Length()) * Vector3::ONE();

		Graphics::BindVertexBuffers(boundingBoxVertexBuffer.get());
		Graphics::BindIndexBuffer(boundingBoxIndexBuffer.get());
		Graphics::SetRenderState(BLEND_REPLACE, CULL_BACK, CMP_LESS_EQUAL, false, false);

		for (size_t i = 0; i < NUM_OCTANT_TASKS; ++i) {
			std::vector<Octant*>& occlusionQueries = octantResults[i].occlusionQueries;

			for (size_t j = 0; j < occlusionQueries.size(); ++j) {
				Octant* octant = occlusionQueries[j];

				const BoundingBox& octantBox = octant->CullingBox();
				BoundingBox box(octantBox.min - enlargement, octantBox.max + enlargement);

				// If bounding box could be clipped by near plane, assume visible without performing query
				if (box.Distance(cameraPosition) < 2.0f * nearClip) {
					octant->OnOcclusionQueryResult(true);
					continue;
				}

				Vector3 size = box.HalfSize();
				Vector3 center = box.Center();

				boxMatrix.m00 = size.x;
				boxMatrix.m11 = size.y;
				boxMatrix.m22 = size.z;
				boxMatrix.m03 = center.x;
				boxMatrix.m13 = center.y;
				boxMatrix.m23 = center.z;

				boundingBoxShaderProgram->SetUniform(U_WORLDMATRIX, boxMatrix);

				unsigned queryId = Graphics::BeginOcclusionQuery(octant);
				Graphics::DrawIndexed(PT_TRIANGLE_LIST, 0, NUM_BOX_INDICES);
				Graphics::EndOcclusionQuery();

				// Remember query in octant to not re-test it until result arrives
				octant->OnOcclusionQuery(queryId);
			}
		}

		previousCameraPosition = cameraPosition;
	}

	void Renderer::DefineFaceSelectionTextures()
	{
		// Face selection textures do not depend on shadow map size. No-op if already defined
		if (faceSelectionTexture) {
			return;
		}

		faceSelectionTexture = std::make_unique<Texture>();
		faceSelectionTexture->Define(TARGET_CUBE_ARRAY, IntVector3 {1, 1, MAX_CUBE_FACES * 2}, FORMAT_RGBA32_SFLOAT_PACK32, 1, 1);
		faceSelectionTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

		const float faceSelectionData1[] =
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};
		const float faceSelectionData2[] =
		{
			-0.5f, 0.5f, 0.5f, 1.5f,
			0.5f, 0.5f, 0.5f, 0.5f,
			-0.5f, 0.5f, 1.5f, 1.5f,
			-0.5f, -0.5f, 1.5f, 0.5f,
			0.5f, 0.5f, 2.5f, 1.5f,
			-0.5f, 0.5f, 2.5f, 0.5f
		};
		const float* const layers[] = {
			faceSelectionData1,
			faceSelectionData2
		};

		for (int layer = 0; layer < std::size(layers); ++layer) {
			for (int face = 0; face < MAX_CUBE_FACES; ++face) {
				faceSelectionTexture->SetData(ImageLevel {
					&layers[layer][face * 4],
					0,
					IntBox {0, 0, 0, 1, 1, 0},
					layer * (int)MAX_CUBE_FACES + face,
					0
				});
			}
		}
	}

	void Renderer::DefineBoundingBoxGeometry()
	{
		const float boxVertexData[] = {
			-1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, 1.0f
		};
		const unsigned short boxIndexData[] = {
			0, 2, 1,
			2, 3, 1,
			2, 4, 3,
			4, 5, 3,
			4, 6, 5,
			6, 7, 5,
			6, 0, 7,
			0, 1, 7,
			4, 2, 0,
			6, 4, 0,
			1, 3, 5,
			1, 5, 7
		};
		const VertexElement elements[] = {
			{ELEM_VECTOR3, ATTR_POSITION}
		};

		boundingBoxVertexBuffer = std::make_unique<VertexBuffer>();
		boundingBoxVertexBuffer->Define(USAGE_DEFAULT, 8, elements, 1, boxVertexData);

		boundingBoxIndexBuffer = std::make_unique<IndexBuffer>();
		boundingBoxIndexBuffer->Define(USAGE_DEFAULT, NUM_BOX_INDICES, sizeof(unsigned short), boxIndexData);

		boundingBoxShaderProgram = Graphics::CreateProgram("BoundingBox.glsl", "", "");
	}

	void Renderer::DefineClusterFrustums()
	{
		Matrix4 cameraProj = camera->ProjectionMatrix(false);
		if (lastClusterFrustumProj != cameraProj) {
			clusterFrustumsDirty = true;
		}

		if (clusterFrustumsDirty) {
			Matrix4 cameraProjInverse = cameraProj.Inverse();
			float cameraNearClip = camera->NearClip();
			float cameraFarClip = camera->FarClip();
			size_t idx = 0;

			float xStep = 2.0f / NUM_CLUSTER_X;
			float yStep = 2.0f / NUM_CLUSTER_Y;
			float zStep = 1.0f / NUM_CLUSTER_Z;

			for (size_t z = 0; z < NUM_CLUSTER_Z; ++z) {
				Vector4 nearVec = cameraProj * Vector4(0.0f, 0.0f, z > 0 ? powf(z * zStep, 2.0f) * cameraFarClip : cameraNearClip, 1.0f);
				Vector4 farVec = cameraProj * Vector4(0.0f, 0.0f, powf((z + 1) * zStep, 2.0f) * cameraFarClip, 1.0f);
				float near = nearVec.z / nearVec.w;
				float far = farVec.z / farVec.w;

				for (size_t y = 0; y < NUM_CLUSTER_Y; ++y) {
					for (size_t x = 0; x < NUM_CLUSTER_X; ++x) {
						Frustum& clusterFrustum = clusterCullData[idx].frustum;
						BoundingBox& clusterBox = clusterCullData[idx].boundingBox;

						clusterFrustum.vertices[0] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, near);
						clusterFrustum.vertices[1] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), near);
						clusterFrustum.vertices[2] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), near);
						clusterFrustum.vertices[3] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, near);
						clusterFrustum.vertices[4] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, far);
						clusterFrustum.vertices[5] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), far);
						clusterFrustum.vertices[6] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), far);
						clusterFrustum.vertices[7] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, far);
						clusterFrustum.UpdatePlanes();
						clusterBox.Define(clusterFrustum);
						++idx;
					}
				}
			}

			lastClusterFrustumProj = cameraProj;
			clusterFrustumsDirty = false;
		}
	}

	void Renderer::CollectOctantsWork(Task* task_, unsigned)
	{
		CollectOctantsTask* task = static_cast<CollectOctantsTask*>(task_);

		// Go through octants in this task's octree branch
		Octant* octant = task->startOctant;
		ThreadOctantResult& result = octantResults[task->resultIdx];

		CollectOctantsAndLights(octant, result);

		// Queue final batch task for leftover nodes if needed
		if (result.drawableAcc) {
			if (result.collectBatchesTasks.size() <= result.batchTaskIdx) {
				result.collectBatchesTasks.push_back(std::make_unique<CollectBatchesTask>(this, &Renderer::CollectBatchesWork));
			}

			CollectBatchesTask* batchTask = result.collectBatchesTasks[result.batchTaskIdx].get();
			batchTask->octants.clear();
			batchTask->octants.insert(batchTask->octants.end(), result.octants.begin() + result.taskOctantIdx, result.octants.end());
			numPendingBatchTasks.fetch_add(1);
			workQueue->QueueTask(batchTask);
		}

		numPendingBatchTasks.fetch_add(-1);
	}

	void Renderer::ProcessLightsWork(Task*, unsigned)
	{
		// Merge the light collection results
		for (size_t i = 0; i < rootLevelOctants.size(); ++i) {
			lights.insert(lights.end(), octantResults[i].lights.begin(), octantResults[i].lights.end());
		}

		// Find the directional light if any
		for (size_t i = 0; i < lights.size(); ) {
			LightDrawable* light = lights[i];
			if (light->GetLightType() == LIGHT_DIRECTIONAL) {
				if (!dirLight || light->GetColor().Average() > dirLight->GetColor().Average()) {
					dirLight = light;
				}
				std::swap(lights.back(), lights.at(i));
				lights.pop_back();
			} else {
				++i;
			}
		}

		// Sort localized lights by increasing distance
		std::sort(lights.begin(), lights.end(), CompareDrawableDistances);

		// Clamp to maximum supported
		if (lights.size() > MAX_LIGHTS) {
			lights.resize(MAX_LIGHTS);
		}

		// Pre-step for shadow map caching: reallocate all lights' shadow map rectangles which are non-zero at this point.
		// If shadow maps were dirtied (size or bias change) reset all allocations instead
		for (size_t i = 0; i < lights.size(); ++i) {
			LightDrawable* light = lights[i];
			if (shadowMapsDirty) {
				light->SetShadowMap(nullptr);
			} else if (drawShadows && light->ShadowStrength() < 1.0f && light->ShadowRect() != IntRect::ZERO()) {
				AllocateShadowMap(light);
			}
		}

		// Check if directional light needs shadows
		if (dirLight) {
			if (shadowMapsDirty) {
				dirLight->SetShadowMap(nullptr);
			}
			if (!drawShadows || dirLight->ShadowStrength() >= 1.0f || !AllocateShadowMap(dirLight)) {
				dirLight->SetShadowMap(nullptr);
			}
		}

		shadowMapsDirty = false;

		size_t lightTaskIdx = 0;

		// Go through lights and setup shadowcaster collection tasks
		for (size_t i = 0; i < lights.size(); ++i) {
			LightDrawable* light = lights[i];
			float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;

			// Light indexing starts at 1 as 0 is "no light"
			lightData[i + 1].position = Vector4(light->WorldPosition(), 1.0f);
			lightData[i + 1].direction = Vector4(-light->WorldDirection(), 0.0f);
			lightData[i + 1].attenuation = Vector4(1.0f / std::max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 1.0f);
			lightData[i + 1].color = light->EffectiveColor();
			lightData[i + 1].viewMask = light->ViewMask();
			lightData[i + 1].shadowParameters = Vector4::ONE(); // Assume unshadowed

			// Check if not shadowcasting or beyond shadow range
			if (!drawShadows || light->ShadowStrength() >= 1.0f) {
				light->SetShadowMap(nullptr);
				continue;
			}

			// Now retry shadow map allocation if necessary. If it's a new allocation, must rerender the shadow map
			if (!light->ShadowMap()) {
				if (!AllocateShadowMap(light)) {
					continue;
				}
			}

			light->InitShadowViews();
			std::vector<ShadowView>& shadowViews = light->ShadowViews();

			// Preallocate shadowcaster list
			ShadowMap& shadowMap = shadowMaps[1];
			size_t casterListIdx = shadowMap.freeCasterListIdx++;
			if (shadowMap.shadowCasters.size() < shadowMap.freeCasterListIdx) {
				shadowMap.shadowCasters.resize(shadowMap.freeCasterListIdx);
			}

			for (size_t j = 0; j < shadowViews.size(); ++j) {
				ShadowView& view = shadowViews[j];

				// Preallocate shadow batch queues
				view.casterListIdx = casterListIdx;

				if (light->IsStatic()) {
					view.staticQueueIdx = shadowMap.freeQueueIdx++;
					view.dynamicQueueIdx = shadowMap.freeQueueIdx++;
				} else {
					view.dynamicQueueIdx = shadowMap.freeQueueIdx++;
				}

				if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx) {
					shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx);
				}

				shadowMap.shadowViews.push_back(&view);
			}

			if (collectShadowCastersTasks.size() <= lightTaskIdx) {
				collectShadowCastersTasks.push_back(std::make_unique<CollectShadowCastersTask>(this, &Renderer::CollectShadowCastersWork));
			}

			collectShadowCastersTasks[lightTaskIdx]->light = light;
			workQueue->AddDependency(processShadowCastersTask.get(), collectShadowCastersTasks[lightTaskIdx].get());
			++lightTaskIdx;
		}

		if (dirLight && dirLight->ShadowMap()) {
			ShadowMap& shadowMap = shadowMaps[0];

			dirLight->InitShadowViews();
			std::vector<ShadowView>& shadowViews = dirLight->ShadowViews();

			for (size_t i = 0; i < shadowViews.size(); ++i) {
				ShadowView& view = shadowViews[i];

				// Directional light needs a new frustum query for each split, as the shadow cameras are typically far outside the main view
				// But queries are only performed later when the shadow map can be focused to visible scene
				view.casterListIdx = shadowMap.freeCasterListIdx++;
				if (shadowMap.shadowCasters.size() < shadowMap.freeCasterListIdx) {
					shadowMap.shadowCasters.resize(shadowMap.freeCasterListIdx);
				}

				view.dynamicQueueIdx = shadowMap.freeQueueIdx++;
				if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx) {
					shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx);
				}

				shadowMap.shadowViews.push_back(&view);
			}
		}

		// Now queue all shadowcaster collection tasks
		if (lightTaskIdx > 0) {
			workQueue->QueueTasks(lightTaskIdx, reinterpret_cast<Task**>(&collectShadowCastersTasks[0]));
		}
	}

	void Renderer::CollectBatchesWork(Task* task_, unsigned threadIndex)
	{
		CollectBatchesTask* task = static_cast<CollectBatchesTask*>(task_);
		ThreadBatchResult& result = batchResults[threadIndex];
		bool threaded = workQueue->NumThreads() > 1;

		std::vector<std::pair<Octant*, unsigned char>>& octants = task->octants;
		std::vector<Batch>& opaqueQueue = threaded ? result.opaqueBatches : opaqueBatches.batches;
		std::vector<Batch>& alphaQueue = threaded ? result.alphaBatches : alphaBatches.batches;

		const Matrix3x4& viewMatrix = camera->ViewMatrix();
		Vector3 viewZ = Vector3(viewMatrix.m20, viewMatrix.m21, viewMatrix.m22);
		Vector3 absViewZ = viewZ.Abs();
		float farClipMul = 32767.0f / camera->FarClip();

		// Scan octants for geometries
		for (size_t i = 0; i < octants.size(); ++i) {
			Octant* octant = octants[i].first;
			unsigned char planeMask = octants[i].second;

			const std::vector<Drawable*>& drawables = octant->Drawables();
			for (size_t j = 0; j < drawables.size(); ++j) {
				Drawable* drawable = drawables[j];

				if (drawable->TestFlag(Drawable::FLAG_GEOMETRY) && (drawable->ViewMask() & viewMask)) {
					const BoundingBox& geometryBox = drawable->WorldBoundingBox();

					// Note: to strike a balance between performance and occlusion accuracy, per-geometry occlusion tests are skipped for now,
					// as octants are already tested with combined actual drawable bounds
					if ((!planeMask || frustum.IsInsideMaskedFast(geometryBox, planeMask)) && drawable->OnPrepareRender(frameNumber, camera)) {
						result.geometryBounds.Merge(geometryBox);

						Vector3 center = geometryBox.Center();
						Vector3 edge = geometryBox.Size() * 0.5f;

						float viewCenterZ = viewZ.DotProduct(center) + viewMatrix.m23;
						float viewEdgeZ = std::max(absViewZ.DotProduct(edge), 0.01f);
						result.minZ = std::min(result.minZ, viewCenterZ - viewEdgeZ);
						result.maxZ = std::max(result.maxZ, viewCenterZ + viewEdgeZ);

						Batch newBatch;

						unsigned distance = static_cast<unsigned>(drawable->Distance() * farClipMul);
						const SourceBatches& batches = static_cast<GeometryDrawable*>(drawable)->Batches();
						size_t numGeometries = batches.NumGeometries();

						for (size_t j = 0; j < numGeometries; ++j) {
							Material* material = batches.GetMaterial(j).get();

							// Assume opaque first
							newBatch.pass = material->GetPass(PASS_OPAQUE);
							newBatch.geometry = batches.GetGeometry(j);
							newBatch.geomIndex = j;
							newBatch.type = drawable->IsGeometryStatic() ? BatchType::Static : BatchType::Complex;
							newBatch.drawableFlags = drawable->Flags();
							newBatch.lightMask = drawable->LightMask();

							if (newBatch.type == BatchType::Static) {
								newBatch.worldTransform = &drawable->WorldTransform();
							} else {
								newBatch.drawable = static_cast<GeometryDrawable*>(drawable);
							}

							if (newBatch.pass) {
								// Perform distance sort in addition to state sort
								if (newBatch.pass->lastSortKey.first != frameNumber || newBatch.pass->lastSortKey.second > distance) {
									newBatch.pass->lastSortKey.first = frameNumber;
									newBatch.pass->lastSortKey.second = distance;
								}
								if (newBatch.geometry->lastSortKey.first != frameNumber || newBatch.geometry->lastSortKey.second > distance + static_cast<unsigned>(j)) {
									newBatch.geometry->lastSortKey.first = frameNumber;
									newBatch.geometry->lastSortKey.second = distance + static_cast<unsigned>(j);
								}
								opaqueQueue.push_back(newBatch);
							} else {
								// If not opaque, try transparent
								newBatch.pass = material->GetPass(PASS_ALPHA);
								if (!newBatch.pass) {
									continue;
								}
								newBatch.distance = drawable->Distance();
								alphaQueue.push_back(newBatch);
							}
						}
					}
				}
			}
		}

		numPendingBatchTasks.fetch_add(-1);
	}

	void Renderer::CollectShadowCastersWork(Task* task, unsigned)
	{
		LightDrawable* light = static_cast<CollectShadowCastersTask*>(task)->light;
		LightType lightType = light->GetLightType();
		std::vector<ShadowView>& shadowViews = light->ShadowViews();

		// Directional lights perform queries later, here only point & spot lights (in shadow atlas) are considered
		ShadowMap& shadowMap = shadowMaps[1];

		if (lightType == LIGHT_POINT) {
			// Point light: perform only one sphere query, then check which of the point light sides are visible
			for (size_t i = 0; i < shadowViews.size(); ++i) {
				// Check if each of the sides is in view. Do not process if isn't.
				// Rendering will be no-op this frame, but cached contents are discarded once comes into view again
				light->SetupShadowView(i, camera);
				ShadowView& view = shadowViews[i];

				if (!frustum.IsInsideFast(BoundingBox(view.shadowFrustum))) {
					view.renderMode = RENDER_STATIC_LIGHT_CACHED;
					view.viewport = IntRect::ZERO();
					view.lastViewport = IntRect::ZERO();
				}
			}

			std::vector<Drawable*>& shadowCasters = shadowMap.shadowCasters[shadowViews[0].casterListIdx];
			octree->FindDrawables(shadowCasters, light->WorldSphere(), Drawable::FLAG_GEOMETRY | Drawable::FLAG_CAST_SHADOWS, light->ShadowViewMask());

		} else if (lightType == LIGHT_SPOT) {
			// Spot light: perform query for the spot frustum
			light->SetupShadowView(0, camera);
			ShadowView& view = shadowViews[0];

			std::vector<Drawable*>& shadowCasters = shadowMap.shadowCasters[view.casterListIdx];
			octree->FindDrawablesMasked(shadowCasters, view.shadowFrustum, Drawable::FLAG_GEOMETRY | Drawable::FLAG_CAST_SHADOWS, light->ShadowViewMask());
		}
	}

	void Renderer::BatchesReadyWork(Task*, unsigned)
	{
	}

	void Renderer::ProcessShadowCastersWork(Task*, unsigned)
	{
		// Queue shadow batch collection tasks.
		// These will also perform shadow batch sorting tasks when done
		if (drawShadows) {
			size_t shadowTaskIdx = 0;
			LightDrawable* lastLight = nullptr;

			for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i) {
				ShadowMap& shadowMap = shadowMaps[i];
				for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j) {
					LightDrawable* light = shadowMap.shadowViews[j]->light;
					// For a point light, make only one task that will handle all of the views and skip rest
					if (light->GetLightType() == LIGHT_POINT && light == lastLight) {
						continue;
					}

					lastLight = light;

					if (collectShadowBatchesTasks.size() <= shadowTaskIdx) {
						collectShadowBatchesTasks.push_back(std::make_unique<CollectShadowBatchesTask>(this, &Renderer::CollectShadowBatchesWork));
					}
					collectShadowBatchesTasks[shadowTaskIdx]->shadowMapIdx = i;
					collectShadowBatchesTasks[shadowTaskIdx]->viewIdx = j;
					numPendingShadowViews[i].fetch_add(1);
					++shadowTaskIdx;
				}
			}

			if (shadowTaskIdx > 0) {
				workQueue->QueueTasks(shadowTaskIdx, reinterpret_cast<Task**>(&collectShadowBatchesTasks[0]));
			}
		}

		// Clear per-cluster light data from previous frame, update cluster frustums and bounding boxes if camera changed,
		// then queue light culling tasks for the needed scene range
		DefineClusterFrustums();
		memset(clusterData.get(), 0, MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z);
		for (size_t z = 0; z < NUM_CLUSTER_Z; ++z) {
			size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
			const Frustum& clusterFrustum = clusterCullData[idx].frustum;
			if (minZ > clusterFrustum.vertices[4].z || maxZ < clusterFrustum.vertices[0].z) {
				continue;
			}
			workQueue->QueueTask(cullLightsTasks[z].get());
		}

		// Finally copy correct shadow matrices for the localized light data
		// Note: directional light shadow matrices may still be pending, but they are not included here
		for (size_t i = 0; i < lights.size(); ++i) {
			LightDrawable* light = lights[i];

			if (light->ShadowMap()) {
				lightData[i + 1].shadowParameters = light->ShadowParameters();
				lightData[i + 1].shadowMatrix = light->ShadowViews()[0].shadowMatrix;
			}
		}
	}

	void Renderer::CollectShadowBatchesWork(Task* task_, unsigned)
	{
		CollectShadowBatchesTask* task = static_cast<CollectShadowBatchesTask*>(task_);
		ShadowMap& shadowMap = shadowMaps[task->shadowMapIdx];
		size_t viewIdx = task->viewIdx;

		for (;;) {
			ShadowView& view = *shadowMap.shadowViews[viewIdx];

			LightDrawable* light = view.light;
			LightType lightType = light->GetLightType();

			float splitMinZ = minZ, splitMaxZ = maxZ;

			// Focus directional light shadow camera to the visible geometry combined bounds, and query for shadowcasters late
			if (lightType == LIGHT_DIRECTIONAL) {
				if (!light->SetupShadowView(viewIdx, camera, light->AutoFocus() ? &geometryBounds : nullptr)) {
					view.viewport = IntRect::ZERO();
				} else {
					splitMinZ = std::max(splitMinZ, view.splitMinZ);
					splitMaxZ = std::min(splitMaxZ, view.splitMaxZ);

					// Before querying (which is potentially expensive), check for degenerate depth range or frustum outside split
					if (splitMinZ >= splitMaxZ || splitMinZ > view.splitMaxZ || splitMaxZ < view.splitMinZ) {
						view.viewport = IntRect::ZERO();
					} else {
						octree->FindDrawablesMasked(shadowMap.shadowCasters[view.casterListIdx], view.shadowFrustum, Drawable::FLAG_GEOMETRY | Drawable::FLAG_CAST_SHADOWS, light->ShadowViewMask());
					}
				}
			}

			// Skip view? (no geometry, out of range or point light face not in view)
			if (view.viewport == IntRect::ZERO()) {
				view.renderMode = RENDER_STATIC_LIGHT_CACHED;
				view.lastViewport = IntRect::ZERO();
			} else {
				const Frustum& shadowFrustum = view.shadowFrustum;
				const Matrix3x4& lightView = view.shadowCamera->ViewMatrix();
				const std::vector<Drawable*>& initialShadowCasters = shadowMap.shadowCasters[view.casterListIdx];

				bool dynamicOrDirLight = lightType == LIGHT_DIRECTIONAL || !light->IsStatic();
				bool dynamicCastersMoved = false;
				bool staticCastersMoved = false;

				size_t totalShadowCasters = 0;
				size_t staticShadowCasters = 0;

				Frustum lightViewFrustum = camera->WorldSplitFrustum(splitMinZ, splitMaxZ).Transformed(lightView);
				BoundingBox lightViewFrustumBox(lightViewFrustum);

				BatchQueue* destStatic = !dynamicOrDirLight ? &shadowMap.shadowBatches[view.staticQueueIdx] : nullptr;
				BatchQueue* destDynamic = &shadowMap.shadowBatches[view.dynamicQueueIdx];

				for (size_t i = 0; i < initialShadowCasters.size(); ++i) {
					Drawable* drawable = initialShadowCasters[i];
					const BoundingBox& geometryBox = drawable->WorldBoundingBox();

					bool inView = drawable->InView(frameNumber);
					bool staticNode = drawable->IsStatic();

					// Check shadowcaster frustum visibility for point lights; may be visible in view, but not in each cube map face
					if (lightType == LIGHT_POINT && !shadowFrustum.IsInsideFast(geometryBox)) {
						continue;
					}

					// Furthermore, check by bounding box extrusion if out-of-view or directional light shadowcaster actually contributes to visible geometry shadowing or if it can be skipped
					// This is done only for dynamic objects or dynamic lights' shadows; cached static shadowmap needs to render everything
					if ((!staticNode || dynamicOrDirLight) && !inView) {
						BoundingBox lightViewBox = geometryBox.Transformed(lightView);

						if (lightType == LIGHT_DIRECTIONAL) {
							lightViewBox.max.z = std::max(lightViewBox.max.z, lightViewFrustumBox.max.z);
							if (!lightViewFrustum.IsInsideFast(lightViewBox)) {
								continue;
							}
						} else {
							// For perspective lights, extrusion direction depends on the position of the shadow caster
							Vector3 center = lightViewBox.Center();
							Ray extrusionRay(center, center);

							float extrusionDistance = view.shadowCamera->FarClip();
							float originalDistance = Clamp(center.Length(), M_EPSILON, extrusionDistance);

							// Because of the perspective, the bounding box must also grow when it is extruded to the distance
							float sizeFactor = extrusionDistance / originalDistance;

							// Calculate the endpoint box and merge it to the original. Because it's axis-aligned, it will be larger
							// than necessary, so the test will be conservative
							Vector3 newCenter = extrusionDistance * extrusionRay.direction;
							Vector3 newHalfSize = lightViewBox.Size() * sizeFactor * 0.5f;
							BoundingBox extrudedBox(newCenter - newHalfSize, newCenter + newHalfSize);
							lightViewBox.Merge(extrudedBox);

							if (!lightViewFrustum.IsInsideFast(lightViewBox)) {
								continue;
							}
						}
					}

					// If not in view, let the node prepare itself for render now
					if (!inView) {
						if (!drawable->OnPrepareRender(frameNumber, camera)) {
							continue;
						}
					}

					++totalShadowCasters;

					if (staticNode) {
						++staticShadowCasters;
						if (drawable->LastUpdateFrameNumber() == frameNumber) {
							staticCastersMoved = true;
						}
					} else {
						if (drawable->LastUpdateFrameNumber() == frameNumber) {
							dynamicCastersMoved = true;
						}
					}

					// If did not allocate a static queue, just put everything to dynamic
					BatchQueue& dest = destStatic ? (staticNode ? *destStatic : *destDynamic) : *destDynamic;
					const SourceBatches& batches = static_cast<GeometryDrawable*>(drawable)->Batches();
					size_t numGeometries = batches.NumGeometries();

					Batch newBatch;

					for (size_t j = 0; j < numGeometries; ++j) {
						Material* material = batches.GetMaterial(j).get();
						newBatch.pass = material->GetPass(PASS_SHADOW);
						if (!newBatch.pass) {
							continue;
						}

						newBatch.geometry = batches.GetGeometry(j);
						newBatch.geomIndex = j;
						newBatch.type = drawable->IsGeometryStatic() ? BatchType::Static : BatchType::Complex;
						newBatch.drawableFlags = drawable->Flags();
						newBatch.lightMask = 0;

						if (newBatch.type == BatchType::Static) {
							newBatch.worldTransform = &drawable->WorldTransform();
						} else {
							newBatch.drawable = static_cast<GeometryDrawable*>(drawable);
						}

						dest.batches.push_back(newBatch);
					}
				}

				// Now determine which kind of caching can be used for the shadow map
				// Dynamic or directional lights
				if (dynamicOrDirLight) {
					// If light atlas allocation changed, light moved, or amount of objects in view changed, render an optimized shadow map
					if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f) || view.lastNumGeometries != totalShadowCasters || dynamicCastersMoved || staticCastersMoved) {
						view.renderMode = RENDER_DYNAMIC_LIGHT;
					} else {
						view.renderMode = RENDER_STATIC_LIGHT_CACHED;
					}
				} else {
					// Static lights
					// If light atlas allocation has changed, or the static light changed, render a full shadow map now that can be cached next frame
					if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f)) {
						view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
					} else {
						view.renderMode = RENDER_STATIC_LIGHT_CACHED;

						// If static shadowcasters updated themselves (e.g. LOD change), render shadow map fully
						// If dynamic casters moved, need to restore shadowmap and rerender
						if (staticCastersMoved) {
							view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
						} else {
							if (dynamicCastersMoved || view.lastNumGeometries != totalShadowCasters) {
								view.renderMode = staticShadowCasters > 0 ? RENDER_STATIC_LIGHT_RESTORE_STATIC : RENDER_DYNAMIC_LIGHT;
							}
						}
					}
				}

				// If no rendering to be done, use the last rendered shadow projection matrix to avoid artifacts when rotating camera
				if (view.renderMode == RENDER_STATIC_LIGHT_CACHED) {
					view.shadowMatrix = view.lastShadowMatrix;
				} else {
					view.lastViewport = view.viewport;
					view.lastNumGeometries = totalShadowCasters;
					view.lastShadowMatrix = view.shadowMatrix;

					// Clear static batch queue if not needed
					if (destStatic && view.renderMode != RENDER_STATIC_LIGHT_STORE_STATIC) {
						destStatic->Clear();
					}
				}
			}

			// For a point light, process all its views in the same task
			if (lightType == LIGHT_POINT && viewIdx < shadowMap.shadowViews.size() - 1 && shadowMap.shadowViews[viewIdx + 1]->light == light) {
				++viewIdx;
			} else {
				break;
			}
		}

		// Sort shadow batches if was the last
		if (numPendingShadowViews[task->shadowMapIdx].fetch_add(-1) == 1) {
			SortShadowBatches(shadowMap);
		}
	}

	void Renderer::CullLightsToFrustumWork(Task* task, unsigned)
	{
		// Cull lights against each cluster frustum on the given Z-level
		size_t z = static_cast<CullLightsTask*>(task)->z;
		const Matrix3x4& cameraView = camera->ViewMatrix();

		// Clear old light data first
		size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
		ClusterCullData* cullData = &clusterCullData[idx];
		for (size_t i = 0; i < NUM_CLUSTER_X * NUM_CLUSTER_Y; ++i) {
			cullData->numLights = 0;
			++cullData;
		}

		// Go through lights and add to each affected cluster. Do culling checks both ways to reduce false positives
		for (size_t i = 0; i < lights.size(); ++i) {
			LightDrawable* light = lights[i];
			LightType lightType = light->GetLightType();

			if (lightType == LIGHT_POINT) {
				Sphere bounds(cameraView * light->WorldPosition(), light->Range());
				float minViewZ = bounds.center.z - light->Range();
				float maxViewZ = bounds.center.z + light->Range();

				idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
				cullData = &clusterCullData[idx];
				if (minViewZ > cullData->frustum.vertices[4].z || maxViewZ < cullData->frustum.vertices[0].z) {
					continue;
				}

				for (size_t y = 0; y < NUM_CLUSTER_Y; ++y) {
					for (size_t x = 0; x < NUM_CLUSTER_X; ++x) {
						if (cullData->numLights < MAX_LIGHTS_CLUSTER) {
							if (bounds.IsInsideFast(cullData->boundingBox) && cullData->frustum.IsInsideFast(bounds)) {
								clusterData[(idx << 4) + cullData->numLights++] = (unsigned char)(i + 1);
							}
						}
						++idx;
						++cullData;
					}
				}

			} else if (lightType == LIGHT_SPOT) {
				Frustum bounds(light->WorldFrustum().Transformed(cameraView));
				BoundingBox boundsBox(bounds);
				float minViewZ = boundsBox.min.z;
				float maxViewZ = boundsBox.max.z;

				idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
				cullData = &clusterCullData[idx];
				if (minViewZ > cullData->frustum.vertices[4].z || maxViewZ < cullData->frustum.vertices[0].z) {
					continue;
				}

				for (size_t y = 0; y < NUM_CLUSTER_Y; ++y) {
					for (size_t x = 0; x < NUM_CLUSTER_X; ++x) {
						if (cullData->numLights < MAX_LIGHTS_CLUSTER) {
							if (bounds.IsInsideFast(cullData->boundingBox) && cullData->frustum.IsInsideFast(boundsBox)) {
								clusterData[(idx << 4) + cullData->numLights++] = (unsigned char)(i + 1);
							}
						}

						++idx;
						++cullData;
					}
				}
			}
		}
	}
}
