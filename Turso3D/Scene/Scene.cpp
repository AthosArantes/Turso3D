#include "Scene.h"
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Scene/SpatialNode.h>

namespace Turso3D
{
	Scene::Scene()
	{
		root.SetScene(this);
	}

	void Scene::Clear()
	{
		root.RemoveAllChildren();
	}
}
