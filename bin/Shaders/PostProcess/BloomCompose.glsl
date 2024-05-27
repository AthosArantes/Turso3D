#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

in vec2 texCoord;
out vec3 fragColor;

uniform sampler2D srcTex0;
uniform sampler2D bloomTex1;
uniform float intensity;

void main()
{
	vec3 color = texture(srcTex0, texCoord).rgb;
	vec3 bloomColor = texture(bloomTex1, texCoord).rgb;
	fragColor = mix(color, bloomColor, intensity);
}

#endif
