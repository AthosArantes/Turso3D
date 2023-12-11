#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

uniform vec2 _translate;
uniform mat4 _transform;

in vec2 inPosition;
in vec4 inColor0;
in vec2 inTexCoord0;

out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
	fragTexCoord = inTexCoord0;
	fragColor = inColor0;

	vec2 translatedPos = inPosition + _translate.xy;
	vec4 outPos = _transform * vec4(translatedPos, 0, 1);

	gl_Position = outPos;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

#ifdef TEXTURED
	uniform sampler2D _tex0;
#endif

void main()
{
#ifdef TEXTURED
	vec4 texColor = texture(_tex0, fragTexCoord);
	finalColor = fragColor * texColor;
#else
	finalColor = fragColor;
#endif
}

#endif