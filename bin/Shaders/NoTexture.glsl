#version 330 core

#include <Uniforms.glsli>

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

#include <Transform.glsli>

in vec3 position;
in vec3 normal;

out vec4 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
noperspective out vec2 vScreenPos;

void main()
{
	mat3x4 world = GetWorldMatrix();

	vWorldPos.xyz = vec4(position, 1.0) * world;
	vNormal = normalize((vec4(normal, 0.0) * world));
	vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;

	gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
	vWorldPos.w = CalculateDepth(gl_Position);
	vScreenPos = CalculateScreenPos(gl_Position);
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

#include <Lighting.glsli>

in vec4 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
noperspective in vec2 vScreenPos;

layout(std140) uniform PerMaterialData3
{
	vec4 BaseColor;
	vec4 AoRoughMetal;
#ifdef EMISSIVE
	vec4 EmissiveParams;
#endif
};

out vec4 fragColor[2];

void main()
{
	//float ao = AoRoughMetal.r;
	float roughness = AoRoughMetal.g;
	float metallic = AoRoughMetal.b;

	vec3 normal = normalize(vNormal);

	// BRDF Shading
	vec3 color = CalculateLighting(vWorldPos, vScreenPos, normal, BaseColor.rgb, metallic, roughness);

#ifdef EMISSIVE
	color += EmissiveParams.rgb * EmissiveParams.a;
#endif

#ifdef OPACITY
	color *= BaseColor.a;
#endif

	// Add environment fog
	fragColor[0] = vec4(mix(fogColor, color, GetFogFactor(vWorldPos.w)), BaseColor.a);
	fragColor[1] = vec4(vViewNormal, 1.0);
}

#endif