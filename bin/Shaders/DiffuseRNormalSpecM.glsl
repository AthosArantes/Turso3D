#version 330 core

#include <Uniforms.glsli>

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

#include <Transform.glsli>

in vec3 position;
in vec3 normal;
in vec4 tangent;
in vec2 texCoord;

out vec4 vWorldPos;
out vec3 vNormal;
out vec4 vTangent;
out vec3 vViewNormal;
out vec2 vTexCoord;
noperspective out vec2 vScreenPos;

void main()
{
	mat3x4 world = GetWorldMatrix();

	vWorldPos.xyz = vec4(position, 1.0) * world;
	vNormal = normalize((vec4(normal, 0.0) * world));
	vTangent = vec4(normalize(vec4(tangent.xyz, 0.0) * world), tangent.w);
	vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;
	vTexCoord = texCoord;

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
#include <Utils.glsli>

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

uniform sampler2D diffuseRoughTex0;
uniform sampler2D normalTex1;
uniform sampler2D specMTex2;

// ====================================================================================================
void main()
{
	// vNt is the tangent space normal
	vec3 vNt = UnpackNormalMap(texture(normalTex1, vTexCoord));
	vec3 vB = vTangent.w * cross(vNormal, vTangent.xyz);
	vec3 normal = normalize(vNt.x * vTangent.xyz + vNt.y * vB + vNt.z * vNormal);

	vec4 sDiffuseRough = texture(diffuseRoughTex0, vTexCoord);
	vec4 sSpecMetal = texture(specMTex2, vTexCoord);

	vec3 albedo = sDiffuseRough.rgb * matDiffColor.rgb;
	vec3 f0 = sSpecMetal.rgb * matSpecColor.rgb;
	float roughness = sDiffuseRough.a * matDiffColor.a;
	float metallic = sSpecMetal.a * matSpecColor.a;

	// BRDF Shading
	vec3 color = albedo * ambientColor.rgb;
	color += CalculateLighting(vWorldPos, vScreenPos, normal, albedo, f0, metallic, roughness);

	// Add environment fog
	fragColor[0] = vec4(mix(fogColor, color, GetFogFactor(vWorldPos.w)), 1.0);
	fragColor[1] = vec4(vViewNormal, 1.0);
}

#endif