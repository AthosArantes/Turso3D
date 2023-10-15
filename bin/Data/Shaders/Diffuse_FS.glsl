#version 330 core

#include "Uniforms.glsli"
#include "Lighting.glsli"

in vec4 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
in vec2 vTexCoord;
noperspective in vec2 vScreenPos;

out vec4 fragColor[2];

layout(std140) uniform PerMaterialData3
{
	vec4 matDiffColor;
	vec4 matSpecColor;
};

uniform sampler2D diffuseTex0;

void main()
{
#ifdef ALPHAMASK
	float alpha = texture(diffuseTex0, vTexCoord).a;
	if (alpha < 0.5) {
		discard;
	}
#endif

	vec3 diffuseLight;
	vec3 specularLight;
	CalculateLighting(vWorldPos, vNormal, vScreenPos, matDiffColor, matSpecColor, diffuseLight, specularLight);

#if 1
	const float gamma = 2.2;
	vec3 diffuseColor = pow(texture(diffuseTex0, vTexCoord).rgb, vec3(gamma));
	vec3 finalColor = diffuseColor * diffuseLight + specularLight;
#else
	vec3 finalColor = texture(diffuseTex0, vTexCoord).rgb * diffuseLight + specularLight;
#endif

	fragColor[0] = vec4(mix(fogColor, finalColor, GetFogFactor(vWorldPos.w)), matDiffColor.a);
	fragColor[1] = vec4(vViewNormal, 1.0);
}
