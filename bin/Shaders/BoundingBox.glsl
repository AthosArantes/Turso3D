#version 330 core

#include <Uniforms.glsli>

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

#include <Transform.glsli>

in vec3 position;

void main()
{
	mat3x4 modelMatrix = GetWorldMatrix();
	vec3 worldPos = vec4(position, 1.0) * modelMatrix;
	gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

out vec4 fragColor;

void main()
{
	fragColor = vec4(1.0, 1.0, 1.0, 0.5);
}

#endif