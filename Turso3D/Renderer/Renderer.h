#pragma once

#include <Turso3D/Graphics/GraphicsDefs.h>
#include <Turso3D/Math/AreaAllocator.h>
#include <Turso3D/Math/Color.h>
#include <Turso3D/Math/Frustum.h>
#include <Turso3D/Renderer/Batch.h>
#include <atomic>
#include <memory>

namespace Turso3D
{
	class Camera;
	class DebugRenderer;
	class Drawable;
	class FrameBuffer;
	class GeometryDrawable;
	class LightDrawable;
	class LightEnvironment;
	class Material;
	class Octant;
	class Octree;
	class RenderBuffer;
	class Scene;
	class ShaderProgram;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;
	class IndexBuffer;
	class WorkQueue;
	struct OcclusionQueryResult;
	struct CollectOctantsTask;
	struct CollectBatchesTask;
	struct CollectShadowBatchesTask;
	struct CollectShadowCastersTask;
	struct CullLightsTask;
	struct ShadowView;
	struct ThreadOctantResult;
	struct Task;

	constexpr int NUM_CLUSTER_X = 16;
	constexpr int NUM_CLUSTER_Y = 8;
	constexpr int NUM_CLUSTER_Z = 8;

	constexpr size_t MAX_LIGHTS = 255;
	constexpr size_t MAX_LIGHTS_CLUSTER = 16;
	constexpr size_t NUM_OCTANT_TASKS = 9;
	constexpr size_t NUM_SHADOW_MAPS = 2; // One for directional lights and another for the rest

	// Texture units with built-in meanings.
	constexpr size_t TU_DIRLIGHTSHADOW = 8;
	constexpr size_t TU_SHADOWATLAS = 9;
	constexpr size_t TU_FACESELECTION = 10;
	constexpr size_t TU_LIGHTCLUSTERDATA = 11;
	constexpr size_t TU_IBL_IEM = 12;
	constexpr size_t TU_IBL_PMREM = 13;
	constexpr size_t TU_IBL_BRDFLUT = 14;

	// Per-thread results for octant collection.
	struct ThreadOctantResult
	{
		// Clear for the next frame.
		void Clear();

		// Drawable accumulator.
		// When full, queue the next batch collection task.
		size_t drawableAcc;
		// Starting octant index for current task.
		size_t taskOctantIdx;
		// Batch collection task index.
		size_t batchTaskIdx;
		// Intermediate octant list.
		std::vector<std::pair<Octant*, unsigned char>> octants;
		// Intermediate light drawable list.
		std::vector<LightDrawable*> lights;
		// Tasks for main view batches collection, queued by the octant collection task when it finishes.
		std::vector<std::unique_ptr<CollectBatchesTask>> collectBatchesTasks;
		// New occlusion queries to be issued.
		std::vector<Octant*> occlusionQueries;
	};

	// Per-thread results for batch collection.
	struct ThreadBatchResult
	{
		// Clear for the next frame.
		void Clear();

		// Minimum geometry Z value.
		float minZ;
		// Maximum geometry Z value.
		float maxZ;
		// Combined bounding box of the visible geometries.
		BoundingBox geometryBounds;
		// Initial opaque batches.
		std::vector<Batch> opaqueBatches;
		// Initial alpha batches.
		std::vector<Batch> alphaBatches;
	};

	// Shadow map data structure.
	// May be shared by several lights.
	struct ShadowMap
	{
		// Default-construct.
		ShadowMap();

		// Clear for the next frame.
		void Clear();

		// Next free batch queue.
		size_t freeQueueIdx;
		// Next free shadowcaster list index.
		size_t freeCasterListIdx;
		// Rectangle allocator.
		AreaAllocator allocator;
		// Shadow map texture.
		std::unique_ptr<Texture> texture;
		// Shadow map framebuffer.
		std::unique_ptr<FrameBuffer> fbo;
		// Shadow views that use this shadow map.
		std::vector<ShadowView*> shadowViews;
		// Shadow batch queues used by the shadow views.
		std::vector<BatchQueue> shadowBatches;
		// Intermediate shadowcaster lists for processing.
		std::vector<std::vector<Drawable*>> shadowCasters;
	};

	// Per-view uniform buffer data.
	struct PerViewUniforms
	{
		// Current camera's view matrix
		Matrix3x4 viewMatrix;
		// Current camera's projection matrix.
		Matrix4 projectionMatrix;
		// Current camera's combined view and projection matrix.
		Matrix4 viewProjMatrix;
		// Current camera's depth parameters.
		Vector4 depthParameters;
		// Current camera's world position.
		Vector4 cameraPosition;
		// Current scene's ambient color.
		Color ambientColor;
		// IBL parameters.
		Vector4 iblParameters;
		// Directional light direction.
		Vector4 dirLightDirection;
		// Directional light color.
		Color dirLightColor;
		// Directional light shadow split parameters.
		Vector4 dirLightShadowSplits;
		// Directional light shadow parameters.
		Vector4 dirLightShadowParameters;
		// Directional light shadow matrices.
		Matrix4 dirLightShadowMatrices[2];
	};

	// Per-light data for cluster light shader.
	struct LightData
	{
		// Light position.
		Vector4 position;
		// Light direction.
		Vector4 direction;
		// Light attenuation parameters.
		Vector4 attenuation;
		// Light color.
		Color color;
		// Light view mask
		unsigned viewMask;
		// Shadow parameters.
		alignas(16) Vector4 shadowParameters;
		// Shadow matrix. For point lights, contains extra parameters.
		Matrix4 shadowMatrix;
	};

	// Per-cluster data for culling lights.
	struct ClusterCullData
	{
		// Cluster frustum.
		Frustum frustum;
		// Cluster bounding box.
		BoundingBox boundingBox;
		// Number of lights already in cluster.
		unsigned char numLights;
	};

	// High-level rendering subsystem.
	// Performs rendering of 3D scenes.
	class Renderer
	{
	public:
		// Construct.
		// WorkQueue and Graphics subsystems must have been initialized.
		Renderer(WorkQueue* workQueue);
		// Destruct.
		~Renderer();

		// Set size and format of shadow maps. First map is used for a directional light, the second as an atlas for others.
		void SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format);
		// Set global depth bias multipiers for shadow maps.
		void SetShadowDepthBiasMul(float depthBiasMul, float slopeScaleBiasMul);
		// Prepare view for rendering. This will utilize worker threads.
		void PrepareView(Scene* scene, Camera* camera, bool drawShadows, bool useOcclusion, float lastFrameTime);
		// Render shadowmaps before rendering the view. Last shadow framebuffer will be left bound.
		void RenderShadowMaps();
		// Clear with fog color and far depth (optional), then render opaque objects into the currently set framebuffer and viewport.
		// If occlusion is used, occlusion queries will also be rendered.
		void RenderOpaque(bool clear = true);
		// Render transparent objects into the currently set framebuffer and viewport.
		void RenderAlpha();

		// Add debug geometry from the objects in frustum into debugRenderer.
		// NOTE: does not automatically render, to allow more geometry to be added elsewhere.
		void RenderDebug(DebugRenderer* debugRenderer);

		// Return a shadow map texture by index for debugging.
		Texture* ShadowMapTexture(size_t index) const;

	private:
		// Collect octants and lights from the octree recursively. Queue batch collection tasks while ongoing.
		void CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, unsigned char planeMask = 0x3f);
		// Add an occlusion query for the octant if applicable.
		void AddOcclusionQuery(Octant* octant, ThreadOctantResult& result, unsigned char planeMask);
		// Allocate shadow map for a light. Return true on success.
		bool AllocateShadowMap(LightDrawable* light);
		// Sort main opaque and alpha batch queues.
		void SortMainBatches();
		// Sort all batch queues of a shadowmap.
		void SortShadowBatches(ShadowMap& shadowMap);
		// Upload light uniform buffer and cluster texture data.
		void UpdateLightData();
		// Render a batch queue.
		void RenderBatches(Camera* camera, const BatchQueue& queue);
		// Check occlusion query results and propagate visibility hierarchically.
		void CheckOcclusionQueries();
		// Render occlusion queries for octants.
		void RenderOcclusionQueries();
		// Define face selection texture for point light shadows.
		void DefineFaceSelectionTextures();
		// Define bounding box geometry for occlusion queries.
		void DefineBoundingBoxGeometry();
		// Setup light cluster frustums and bounding boxes if necessary.
		void DefineClusterFrustums();
		// Work function to collect octants.
		void CollectOctantsWork(Task* task, unsigned threadIndex);
		// Process lights collected by octant tasks, and queue shadowcaster query tasks for them as necessary.
		void ProcessLightsWork(Task* task, unsigned threadIndex);
		// Work function to collect main view batches from geometries.
		void CollectBatchesWork(Task* task, unsigned threadIndex);
		// Work function to collect shadowcasters per shadowcasting light.
		void CollectShadowCastersWork(Task* task, unsigned threadIndex);
		// Work function for dummy task that signals batches are ready for sorting.
		void BatchesReadyWork(Task* task, unsigned threadIndex);
		// Work function to queue shadowcaster batch collection tasks. Requires batch collection and shadowcaster query tasks to be complete.
		void ProcessShadowCastersWork(Task* task, unsigned threadIndex);
		// Work function to collect shadowcaster batches per shadow view.
		void CollectShadowBatchesWork(Task* task, unsigned threadIndex);
		// Work function to cull lights against a Z-slice of the frustum grid.
		void CullLightsToFrustumWork(Task* task, unsigned threadIndex);

	private:
		// Cached work queue subsystem.
		WorkQueue* workQueue;

		// Current scene.
		Scene* scene;
		// Current scene octree.
		Octree* octree;
		// Current scene light environment.
		LightEnvironment* lightEnvironment;
		// Camera used to render the current scene.
		Camera* camera;
		// Camera frustum.
		Frustum frustum;
		// Camera view mask.
		unsigned viewMask;
		// Framenumber.
		unsigned short frameNumber;
		// Shadow use flag.
		bool drawShadows;
		// Occlusion use flag.
		bool useOcclusion;
		// Shadow maps globally dirty flag.
		// All cached shadow content should be reset.
		bool shadowMapsDirty;
		// Cluster frustums dirty flag.
		bool clusterFrustumsDirty;
		// Previous frame camera position for occlusion culling bounding box elongation.
		Vector3 previousCameraPosition;
		// Last frame time for occlusion query staggering.
		float lastFrameTime;
		// Container for holding occlusion query results.
		std::vector<OcclusionQueryResult> occlusionQueryResults;
		// Root-level octants, used as a starting point for octant and batch collection.
		// The root octant is included if it also contains drawables.
		std::vector<Octant*> rootLevelOctants;
		// Counter for batch collection tasks remaining.
		// When zero, main batch sorting can begin while other tasks go on.
		std::atomic<int> numPendingBatchTasks;
		// Counters for shadow views remaining per shadowmap.
		// When zero, the shadow batches can be sorted.
		std::atomic<int> numPendingShadowViews[2];
		// Per-octree branch octant collection results.
		std::unique_ptr<ThreadOctantResult[]> octantResults;
		// Per-worker thread batch collection results.
		std::unique_ptr<ThreadBatchResult[]> batchResults;
		// Minimum Z value for all geometries in frustum.
		float minZ;
		// Maximum Z value for all geometries in frustum.
		float maxZ;
		// Combined bounding box of the visible geometries.
		BoundingBox geometryBounds;
		// Brightest directional light in frustum.
		LightDrawable* dirLight;
		// Accepted point and spot lights in frustum.
		std::vector<LightDrawable*> lights;
		// Shadow maps.
		std::unique_ptr<ShadowMap[]> shadowMaps;
		// Opaque batches.
		BatchQueue opaqueBatches;
		// Transparent batches.
		BatchQueue alphaBatches;
		// Last camera used for rendering.
		Camera* lastCamera;
		// Last material pass used for rendering.
		Pass* lastPass;
		// Last material used for rendering.
		Material* lastMaterial;
		// Constant depth bias multiplier.
		float depthBiasMul;
		// Slope-scaled depth bias multiplier.
		float slopeScaleBiasMul;
		// Last projection matrix used to initialize cluster frustums.
		Matrix4 lastClusterFrustumProj;
		// Cluster frustums, bounding boxes and number of found lights.
		std::unique_ptr<ClusterCullData[]> clusterCullData;
		// Cluster uniform buffer data CPU copy.
		std::unique_ptr<uint8_t[]> clusterData;
		// Light uniform buffer data CPU copy.
		std::unique_ptr<LightData[]> lightData;
		// Per-view uniform buffer data CPU copy.
		PerViewUniforms perViewData;
		// Frustum SAT test data for verifying whether to add an occlusion query.
		SATData frustumSATData;
		// Tasks for octant collection.
		std::unique_ptr<CollectOctantsTask> collectOctantsTasks[NUM_OCTANT_TASKS];
		// Task for light processing.
		std::unique_ptr<Task> processLightsTask;
		// Tasks for shadow light processing.
		std::vector<std::unique_ptr<CollectShadowCastersTask>> collectShadowCastersTasks;
		// Dummy task to ensure batches have been collected.
		std::unique_ptr<Task> batchesReadyTask;
		// Task for queuing shadow views for further processing.
		std::unique_ptr<Task> processShadowCastersTask;
		// Tasks for shadow batch processing.
		std::vector<std::unique_ptr<CollectShadowBatchesTask>> collectShadowBatchesTasks;
		// Tasks for light grid culling.
		std::unique_ptr<CullLightsTask> cullLightsTasks[NUM_CLUSTER_Z];

		// Face selection UV indirection texture array.
		std::unique_ptr<Texture> faceSelectionTexture;
		// Cluster lookup 3D texture.
		std::unique_ptr<Texture> clusterTexture;
		// Per-view uniform buffer.
		std::unique_ptr<UniformBuffer> perViewDataBuffer;
		// Light data uniform buffer.
		std::unique_ptr<UniformBuffer> lightDataBuffer;
		// Bounding box vertex buffer.
		std::unique_ptr<VertexBuffer> boundingBoxVertexBuffer;
		// Bounding box index buffer.
		std::unique_ptr<IndexBuffer> boundingBoxIndexBuffer;
		// Cached bounding box shader program.
		std::shared_ptr<ShaderProgram> boundingBoxShaderProgram;
		// Cached static object shadow buffer.
		// Note: only needed for the light atlas, not the directional light shadowmap.
		std::unique_ptr<RenderBuffer> staticObjectShadowBuffer;
		// Cached static object shadow framebuffer.
		std::unique_ptr<FrameBuffer> staticObjectShadowFbo;

		// Instancing vertex buffer.
		std::unique_ptr<VertexBuffer> instanceVertexBuffer;
		// Instance transforms for opaque and alpha batches.
		std::vector<Matrix3x4> instanceTransforms;
	};
}
