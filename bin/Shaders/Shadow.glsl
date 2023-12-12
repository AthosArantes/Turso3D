#version 330 core

#include <Uniforms.glsli>

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

#include <Transform.glsli>

in vec3 position;

#ifdef ALPHAMASK
	in vec2 texCoord;
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

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

#ifdef ALPHAMASK
	in vec2 vTexCoord;

	uniform sampler2D diffuseTex0;
#endif

out vec4 fragColor;

void main()
{
#ifdef ALPHAMASK
	float alpha = texture(diffuseTex0, vTexCoord).a;
	if (alpha < 0.5) discard;
#endif
	fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}

#endif