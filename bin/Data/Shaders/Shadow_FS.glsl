#version 330 core

#include "Uniforms.glsli"

#ifdef ALPHAMASK
	in vec2 vTexCoord;
#endif

#ifdef ALPHAMASK
	uniform sampler2D diffuseTex0;
#endif

out vec4 fragColor;

void main()
{
#ifdef ALPHAMASK
	float alpha = texture(diffuseTex0, vTexCoord).a;
	if (alpha < 0.5) {
		discard;
	}
#endif
	fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
