#version 330 core

#include <uniforms.h>

#pragma shader:VS //===============================================================================
#include <transform.h>

in vec3 position;
in vec3 normal;
in vec4 tangent;
in vec2 texCoord;

out vec4 vWorldPos;
out vec3 vNormal;
out vec4 vTangent;
out vec3 vBiTangent;
out vec3 vViewNormal;
out vec2 vTexCoord;
noperspective out vec2 vScreenPos;

void main()
{
	mat3x4 world = GetWorldMatrix();

	vWorldPos.xyz = vec4(position, 1.0) * world;
	vNormal = normalize((vec4(normal, 0.0) * world));
	vTangent = vec4(normalize(vec4(tangent.xyz, 0.0) * world), tangent.w);
	vBiTangent = vTangent.w * cross(vNormal, vTangent.xyz);
	vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;
	vTexCoord = texCoord;

	gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
	vWorldPos.w = CalculateDepth(gl_Position);
	vScreenPos = CalculateScreenPos(gl_Position);
}

#pragma shader:FS //===============================================================================
#include <pbr.h>
#include <utils.h>

uniform sampler2D albedoTex0;
uniform sampler2D aoRoughMetalTex1;
uniform sampler2D normalTex2;
uniform sampler2D detailAlbedoTex3;
uniform sampler2D detailNormalTex4;
#ifdef EMISSIVE
	uniform sampler2D emissiveTex5; // Alpha is used as emission mask
#endif

in vec4 vWorldPos;
in vec3 vNormal;
in vec4 vTangent;
in vec3 vBiTangent;
in vec3 vViewNormal;
in vec2 vTexCoord;
noperspective in vec2 vScreenPos;

layout(std140) uniform PerMaterialData3
{
	vec4 BaseColor;
	vec4 AoRoughMetal;
	vec4 DetailParams; // x = Number of repetitions; y = Intensity multiplier
#ifdef EMISSIVE
	vec4 EmissiveParams; // RGB = Color of emission, A = intensity
#endif
};

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 fragNormal;

void main()
{
	vec4 sAlbedo = texture(albedoTex0, vTexCoord);
	vec4 sAoRoughMetal = texture(aoRoughMetalTex1, vTexCoord);

	vec4 sDetailAlbedo = texture(detailAlbedoTex3, vTexCoord * DetailParams.x);

	float ao = sAoRoughMetal.r * AoRoughMetal.r;
	float roughness = clamp(sAoRoughMetal.g * AoRoughMetal.g, 0.01, 0.99);
	float metallic = sAoRoughMetal.b * AoRoughMetal.b;

#ifdef TINTMASK
	float tintMask = sAoRoughMetal.a * AoRoughMetal.a;
	vec3 albedo = mix(sAlbedo.rgb, sAlbedo.rgb * BaseColor.rgb, tintMask);
#else
	vec3 albedo = sAlbedo.rgb * BaseColor.rgb;
#endif

	albedo = mix(albedo, albedo * sDetailAlbedo.rgb, DetailParams.y);

#if defined(ALPHAMASK) || defined(OPACITY)
	const float alpha = sAlbedo.a * BaseColor.a;
#else
	const float alpha = 1.0;
#endif

#ifdef ALPHAMASK
	if (alpha < 0.5) discard;
#endif

	vec3 baseNormal = BC5NormalMap(texture(normalTex2, vTexCoord));
	vec3 detailNormal = BC5NormalMap(texture(detailNormalTex4, vTexCoord * DetailParams.x)) * DetailParams.y;
	vec3 blendedNormal = BlendNormalMap(baseNormal * 0.5 + 0.5, detailNormal * 0.5 + 0.5);
	vec3 normal = TangentSpaceNormal(blendedNormal, vTangent, vBiTangent, vNormal);

	// BRDF Shading
	vec3 color = CalculateLighting(vWorldPos, vScreenPos, normal, albedo, metallic, roughness);
	color *= ao;

#ifdef EMISSIVE
	vec4 sEmissive = texture(emissiveTex5, vTexCoord);
	color += sEmissive.rgb * EmissiveParams.rgb * sEmissive.a * EmissiveParams.a;
#endif

	fragColor = vec4(color, alpha);
	fragNormal = vec4(vec4(normal, 0.0) * viewMatrix * 0.5 + 0.5, 0.0);
}
