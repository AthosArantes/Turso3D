## Experimental Turso3D modified code
This is an experimental fork of [Turso3D](https://github.com/cadaver/turso3d)

#### Major differences from the original project:
 - SDL2 replaced by GLFW;
 - Custom smart pointers replaced by the std ones;
 - Data is stored in xml (handled by pugixml), not in json;
 - Original Turso3D/Urho3D models needs to be converted (Model converter is included), because all indices are read as unsigned;
 - RTTI completely removed;
 - Event system completely removed for now;
 - Object Factory completely removed for now;

#### Notes
C++ 17 compiler required.

There are other changes such as the scene no longer being a node, but a "container" for the root node and the octree (which also is no longer a node).

# Turso3D

Experimental 3D / game engine technology partially based on the Urho3D codebase. Expected to remain in an immature or "toy" state for the time being.

- OpenGL 3.2 / GLFW
- Forward+ rendering, currently up to 255 lights in view
- Threaded work queue to speed up animation and view preparation
- Caching of static shadow maps
- Hardware occlusion queries that work on the octree hierarchy
- SSAO