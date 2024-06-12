#include <Turso3D/Scene/Scene.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Scene/SpatialNode.h>

namespace Turso3D
{
	Scene::Scene(WorkQueue* workQueue, Graphics* graphics) :
		octree(workQueue, graphics)
	{
		root.SetScene(this);
	}

	void Scene::Clear()
	{
		root.DestroyAllChildren();
	}
}
