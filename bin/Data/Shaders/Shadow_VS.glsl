#version 330 core

#include "Uniforms.glsli"
#include "Transform.glsli"

in vec3 position;

#ifdef ALPHAMASK
	in vec2 texCoord;
#endif

#ifdef ALPHAMASK
	out vec2 vTexCoord;
#endif

void main()
{
	mat3x4 world = GetWorldMatrix();

#ifdef ALPHAMASK
	vTexCoord = texCoord;
#endif

	vec3 worldPos = vec4(position, 1.0) * world;
	gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}
