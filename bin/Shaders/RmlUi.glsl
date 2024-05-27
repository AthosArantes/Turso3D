#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec2 position;
in vec4 color;
in vec2 texCoord;

#ifdef TEXTURED
out vec2 vTexCoord;
#endif
out vec4 vColor;

uniform vec2 translate;
uniform mat4 transform;

void main()
{
#ifdef TEXTURED
	vTexCoord = texCoord;
#endif
	vColor = color;

	vec2 translatedPos = position + translate.xy;
	vec4 outPos = transform * vec4(translatedPos, 0.0, 1.0);

	gl_Position = outPos;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

#ifdef TEXTURED
uniform sampler2D tex0;
#endif

#ifdef TEXTURED
in vec2 vTexCoord;
#endif
in vec4 vColor;

out vec4 finalColor;

void main()
{
#ifdef TEXTURED
	vec4 texColor = texture(tex0, vTexCoord);
	finalColor = vColor * texColor;
#else
	finalColor = vColor;
#endif
}

#endif