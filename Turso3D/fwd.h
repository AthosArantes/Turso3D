#pragma once

namespace Turso3D
{
	// Core

	class WorkQueue;

	// IO

	class FileStream;
	class MemoryStream;
	class Stream;

	// Graphics

	class FrameBuffer;
	class IndexBuffer;
	class RenderBuffer;
	class Shader;
	class ShaderProgram;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;

	// Math

	class AreaAllocator;
	class BoundingBox;
	class Color;
	class Frustum;
	class IntBox;
	class IntRect;
	class IntVector2;
	class IntVector3;
	class Matrix3;
	class Matrix3x4;
	class Matrix4;
	class Plane;
	class Polyhedron;
	class Quaternion;
	class Ray;
	class Rect;
	class Sphere;
	class Vector2;
	class Vector3;
	class Vector4;

	// Renderer

	struct AnimationKeyFrame;
	struct AnimationStateTrack;
	struct AnimationTrack;
	struct Batch;
	struct BatchQueue;
	struct DebugVertex;
	struct Geometry;
	struct ModelBone;
	struct RaycastResult;
	struct ShadowView;

	class AnimatedModel;
	class AnimatedModelDrawable;
	class Animation;
	class AnimationState;
	class Bone;
	class Camera;
	class CombinedBuffer;
	class DebugRenderer;
	class Drawable;
	class GeometryDrawable;
	class GeometryNode;
	class Light;
	class LightDrawable;
	class LightEnvironment;
	class Material;
	class Model;
	class Octant;
	class Octree;
	class OctreeNode;
	class OctreeNodeBase;
	class Pass;
	class Renderer;
	class SourceBatches;
	class StaticModel;
	class StaticModelDrawable;

	// Resource

	class Resource;
	class ResourceCache;

	// Scene

	class Node;
	class Scene;
	class SpatialNode;

	// Utils

	struct StringHash;
}
