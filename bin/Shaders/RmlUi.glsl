#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

uniform vec2 uTranslate;
uniform mat4 uTransform;

in vec2 position;
in vec4 color;
in vec2 texCoord;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
	vTexCoord = texCoord;
	vColor = color;

	vec2 translatedPos = position + uTranslate.xy;
	vec4 outPos = uTransform * vec4(translatedPos, 0.0, 1.0);

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

in vec2 vTexCoord;
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