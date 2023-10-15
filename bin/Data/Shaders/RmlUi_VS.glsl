#version 330 core

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
