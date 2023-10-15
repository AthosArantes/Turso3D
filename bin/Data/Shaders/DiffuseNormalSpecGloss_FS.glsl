#version 330 core

#include "Uniforms.glsli"
#include "Lighting.glsli"

in vec4 vWorldPos;
in vec3 vNormal;
in vec4 vTangent;
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
uniform sampler2D normalTex1;
uniform sampler2D specglossTex2;

void main()
{
#ifdef ALPHAMASK
	float alpha = texture(diffuseTex0, vTexCoord).a;
	if (alpha < 0.5) {
		discard;
	}
#endif

	// vNt is the tangent space normal
	vec3 vNt = texture(normalTex1, vTexCoord).rgb * 2.0 - 1.0;
	vec3 vB = vTangent.w * cross(vNormal, vTangent.xyz);
	vec3 normal = normalize(vNt.x * vTangent.xyz + vNt.y * vB + vNt.z * vNormal);

	vec4 matSpecGloss = matSpecColor * texture(specglossTex2, vTexCoord);

	vec3 diffuseLight;
	vec3 specularLight;
	CalculateLighting(vWorldPos, normal, vScreenPos, matDiffColor, matSpecGloss, diffuseLight, specularLight);

#if 1
	// Convert sRGB to Linear
	const float gamma = 2.2;
	vec3 diffuseColor = pow(texture(diffuseTex0, vTexCoord).rgb, vec3(gamma));
	vec3 finalColor = diffuseColor * diffuseLight + specularLight;
#else
	vec3 finalColor = texture(diffuseTex0, vTexCoord).rgb * diffuseLight + specularLight;
#endif

	fragColor[0] = vec4(mix(fogColor, finalColor, GetFogFactor(vWorldPos.w)), matDiffColor.a);
	fragColor[1] = vec4(vViewNormal, 1.0);
}
