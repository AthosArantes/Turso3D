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

uniform sampler2D srcTex0;
uniform float threshold;

in vec2 texCoord;
out vec3 fragColor;

void main()
{
	vec3 color = texture(srcTex0, texCoord).rgb;
	float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
	fragColor = color * step(threshold, lum);
}

#endif
