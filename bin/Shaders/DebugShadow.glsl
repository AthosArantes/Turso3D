#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

uniform mat4 worldViewProjMatrix;

in vec3 position;
in vec2 texCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = texCoord;
	gl_Position = vec4(position, 1.0) * worldViewProjMatrix;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D diffuseTex0;

void main()
{
	float depth = texture(diffuseTex0, vTexCoord).r;
	// Raise to 2nd power to see differences better
	depth *= depth;
	fragColor = vec4(depth, depth, depth, 1.0);
}

#endif