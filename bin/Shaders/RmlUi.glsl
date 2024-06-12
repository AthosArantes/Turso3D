#version 330 core

#pragma shader vs //===============================================================================
in vec2 position;
in vec4 color;
in vec2 texCoord;

out vec2 vTexCoord;
out vec4 vColor;

uniform vec2 translate;
uniform mat4 transform;

void main()
{
	vTexCoord = texCoord;
	vColor = color;

	vec2 translatedPos = position + translate.xy;
	vec4 outPos = transform * vec4(translatedPos, 0.0, 1.0);

	gl_Position = outPos;
}

#pragma shader fs //===============================================================================
#ifdef TEXTURED
uniform sampler2D tex0;
#endif

in vec2 vTexCoord;
in vec4 vColor;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 fragMask;

void main()
{
#ifdef TEXTURED
	vec4 color = texture(tex0, vTexCoord);
	color *= vColor;

	float mask = 1.0 - step(0.0, -color.a);
#else
	vec4 color = vColor;
	float mask = 1.0;
#endif

	fragColor = color;
	fragMask = vec4(mask);
}
