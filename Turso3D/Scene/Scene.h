#pragma once

#include <Turso3D/Renderer/LightEnvironment.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Scene/Node.h>

namespace Turso3D
{
	class WorkQueue;

	class Scene
	{
	public:
		// WorkQueue and Graphics subsystems must have been initialized, as it's required by Octree.
		Scene(WorkQueue* workQueue);

		// Destroy child nodes recursively, leaving the scene empty.
		void Clear();

		// Return the scene's root node.
		Node* GetRoot() { return &root; }
		// Return environment lighting
		LightEnvironment* GetEnvironmentLighting() { return &lighting; }

		// Return the scene's octree.
		Octree* GetOctree() { return &octree; }

	private:
		// The root node.
		Node root;
		// The scene environment lighting
		LightEnvironment lighting;

		// The octree used for rendering drawables.
		Octree octree;
	};
}
