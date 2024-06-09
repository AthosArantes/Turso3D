## Experimental Turso3D modified code
This is an experimental fork of [Turso3D](https://github.com/cadaver/turso3d)

#### Major differences from the original project:
- SDL2 replaced by GLFW;
- Custom smart pointers replaced by the std ones;
- Data is stored in xml only;
- Original Turso3D/Urho3D models needs to be converted (Model converter is included), because all indices are read as unsigned;
- RTTI completely removed;
- Event system completely removed for now;
- Object Factory completely removed for now;
- Defaults to a PBR IBL based shader;

#### Notes

C++ 17 compiler required.  
Development under Visual Studio 2022

There are other changes such as the scene no longer being a node, but a "container" for the root node and the octree (which also is no longer a node).  
The opengl required version was bumped to **3.3 core**, with few required extensions.

#### Dependencies
- [GLFW](https://www.glfw.org)
- [GLEW](https://github.com/nigels-com/glew)
- [pugixml](https://github.com/zeux/pugixml)
- [fmt](https://github.com/fmtlib/fmt)
- [RmlUi](https://github.com/mikke89/RmlUi)
- - [FreeType](https://freetype.org) *[1]
- [gli](https://github.com/g-truc/gli) *[2]
- - [glm](https://github.com/g-truc/glm) *[3]

*[1] - FreeType used only by RmlUi*  
*[2] - GLI code was modified to not use 'using namespace glm'*  
*[3] - GLM used only by GLI*  

# Turso3D

Experimental 3D / game engine technology partially based on the Urho3D codebase. Expected to remain in an immature or "toy" state for the time being.

- OpenGL 3.3 / GLFW
- Forward+ rendering, currently up to 255 lights in view
- Threaded work queue to speed up animation and view preparation
- Caching of static shadow maps
- Hardware occlusion queries that work on the octree hierarchy
- SSAO