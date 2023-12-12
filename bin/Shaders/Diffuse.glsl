#version 330 core

#include <Uniforms.glsli>

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

#include <Transform.glsli>

in vec3 position;
in vec3 normal;
in vec2 texCoord;

out vec4 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
out vec2 vTexCoord;
noperspective out vec2 vScreenPos;

void main()
{
	mat3x4 world = GetWorldMatrix();

	vWorldPos.xyz = vec4(position, 1.0) * world;
	vNormal = normalize((vec4(normal, 0.0) * world));
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

// ====================================================================================================
void main()
{
	vec4 sDiffuse = texture(diffuseTex0, vTexCoord);

#ifdef ALPHAMASK
	if (sDiffuse.a < 0.5) {
		discard;
	}
#endif

	vec3 albedo = sDiffuse.rgb * matDiffColor.rgb;
	vec3 normal = normalize(vNormal);
	vec3 f0 = matSpecColor.rgb;
	float metallic = matSpecColor.a;
#ifdef ALPHAMASK
	float roughness = matDiffColor.a;
#else
	float roughness = sDiffuse.a * matDiffColor.a;
#endif

	// BRDF Shading
	vec3 color = albedo * ambientColor.rgb;
	color += CalculateLighting(vWorldPos, vScreenPos, normal, albedo, f0, metallic, roughness);

	// Add environment fog
	fragColor[0] = vec4(mix(fogColor, color, GetFogFactor(vWorldPos.w)), 1.0);
	fragColor[1] = vec4(vViewNormal, 1.0);
}

#endif