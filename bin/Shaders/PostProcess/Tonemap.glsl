#version 330 core

#pragma shader:VS //===============================================================================
in vec3 position;

out vec2 texCoord;

void main()
{
	texCoord = vec2(position.xy) * 0.5 + 0.5;
	gl_Position = vec4(position, 1.0);
}

#pragma shader:FS //===============================================================================
#include <Tonemap.h>

in vec2 texCoord;

uniform sampler2D diffuseTex0;
uniform float exposure;

out vec4 fragColor;

void main()
{
	vec3 hdr = texture(diffuseTex0, texCoord).rgb;
	hdr *= exposure;

	//vec3 ldr = LinearToneMapping(hdr);
	vec3 ldr = HableToneMapping(hdr);
	//vec3 ldr = ACESFitted(hdr);

	fragColor = vec4(ldr, 1.0);
}
