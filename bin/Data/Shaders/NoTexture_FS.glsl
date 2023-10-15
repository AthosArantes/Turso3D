#include "Uniforms.glsli"
#include "Lighting.glsli"

in vec4 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
noperspective in vec2 vScreenPos;
out vec4 fragColor[2];

layout(std140) uniform PerMaterialData3
{
	vec4 matDiffColor;
	vec4 matSpecColor;
};

void main()
{
	vec3 diffuseLight;
	vec3 specularLight;
	CalculateLighting(vWorldPos, vNormal, vScreenPos, matDiffColor, matSpecColor, diffuseLight, specularLight);

	vec3 finalColor = diffuseLight + specularLight;

	fragColor[0] = vec4(mix(fogColor, finalColor, GetFogFactor(vWorldPos.w)), matDiffColor.a);
	fragColor[1] = vec4(vViewNormal, 1.0);
}
