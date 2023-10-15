#version 330 core

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

#ifdef TEXTURED
	uniform sampler2D _tex;
#endif

void main()
{
#ifdef TEXTURED
	vec4 texColor = texture(_tex, fragTexCoord);
	finalColor = fragColor * texColor;
#else
	finalColor = fragColor;
#endif
}
