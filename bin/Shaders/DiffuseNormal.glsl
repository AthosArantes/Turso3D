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

	//vec3 bitangent = cross(tangent.xyz, normal) * tangent.w;
	//vTexCoord = vec3(GetTexCoord(texCoord), bitangent.xy);
	//vTangent = vec4(tangent.xyz, bitangent.z);
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

#define EXCLUDE_DEFAULT_CALC_LIGHTING

#include <Lighting.glsli>
#include <BRDF.glsli>

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
#ifdef NORMALMAP
uniform sampler2D normalTex1;
#endif
#ifdef SPECGLOSSMAP
uniform sampler2D specglossTex2;
#endif

// ====================================================================================================

const float MIRROR = 0.0;
const float AIR = 1.0;
const float BUBBLE = 1.1;
const float ICE = 1.31;
const float WATER = 1.33;
const float GLASS = 1.5;
const float STANDARD = 2.0;
const float STEEL = 2.5;

vec3 CookTorrance(float NdotL, float NdotH, float NdotV, vec3 lightColor, float roughness, float ior, float attenuation)
{
	float NH2 = pow(NdotH, 2.0);
	//float tan2Alpha = (NH2 - 1.0) / NH2;
	float roughness2 = pow(clamp(roughness, 0.01, 0.99), 2.0);
	float Kr = pow((1.0 - ior) / (1.0 + ior), 2.0);
	//Kr = mix(Kr, materialColour.rgb, metallic);

	float F = F_Shlick(NdotV, Kr);
	//float F = F_CookTorrance(VdotH, Kr);
	float D = D_TrowbridgeReitz(NH2, roughness2);
	//float D = D_Beckmann(NH2, tan2Alpha, roughness2);
	float G = G_GGXSmith(roughness2, NdotV, NdotL);
	//float G = G_CookTorrance(NH2, NdotV, NdotL, VdotH);

	return lightColor * ((F * G * D) / ((PI * NdotV))) * attenuation;
}

void CalculateDirLight(vec4 worldPos, vec3 normal, inout vec3 diffuseLight, inout vec3 specularLight)
{
	vec3 camDir = normalize(cameraPosition - vWorldPos.xyz);
	float roughness = matSpecColor.a;

	vec3 lightDir = normalize(dirLightDirection);
	vec3 halfAngle = normalize(camDir + lightDir);
	float NdotL = max(dot(normal, lightDir), 0.0);
	float NdotH = max(dot(normal, halfAngle), 0.0);
	float NdotV = max(dot(normal, camDir), 0.0);
	//float VdotH = max(dot(camDir, halfAngle), 0.0);
	//float LdotV = max(dot(camDir, lightDir), 0.0);

	if (NdotL <= 0.0) return;
	NdotL = clamp(NdotL, 0.0, 1.0);

	float shadow = SampleDirectionalShadow(worldPos);

	specularLight += CookTorrance(NdotL, NdotH, NdotV, dirLightColor.rgb, roughness, STANDARD, dirLightColor.a) * shadow;
	//diffuseLight += (dirLightColor.rgb * NdotL) * (1.0 - roughness) * shadow;
	diffuseLight += dirLightColor.rgb * NdotL * shadow;

	//float gloss = (1.0 - roughness);
	//diffuseLight += ((dirLightColor.rgb * NdotL) + OrenNayarDiffuse(LdotV, NdotL, NdotV, roughness, 1.0) * gloss) * shadow;
	//diffuseLight += (dirLightColor.rgb * NdotL) * OrenNayarDiffuse(LdotV, NdotL, NdotV, roughness, 1.0) * shadow;
	//diffuseLight += (dirLightColor.rgb * NdotL) * OrenNayarDiffuse(LdotV, NdotL, NdotV, roughness, shadow);
}

void CalculateLight(uint index, vec4 worldPos, vec3 normal, inout vec3 diffuseLight, inout vec3 specularLight)
{
	//if (index == 0U) return;

	vec3 camDir = normalize(cameraPosition - vWorldPos.xyz);
	float roughness = matSpecColor.a;

	vec3 lightPosition = lights[index].position.xyz;
	vec4 lightAttenuation = lights[index].attenuation;
	vec4 lightColor = lights[index].color;
	vec3 lightDirection = lights[index].direction.xyz;

	vec3 lightVec = lightPosition - worldPos.xyz;
	vec3 scaledLightVec = lightVec * lightAttenuation.x;
	vec3 lightDir = normalize(lightVec);
	vec3 halfAngle = normalize(camDir + lightDir);
	float attenuation = 1.0 - dot(scaledLightVec, scaledLightVec);
	float NdotL = dot(normal, lightDir);
	float NdotH = max(dot(normal, halfAngle), 0.0);
	float NdotV = max(dot(normal, camDir), 0.0);
	//float VdotH = max(dot(camDir, halfAngle), 0.0);
	//float LdotV = max(dot(camDir, lightDir), 0.0);

	if (attenuation <= 0.0 || NdotL <= 0.0) return;

	NdotL = clamp(NdotL, 0.0, 1.0);
	//NdotL = pow(NdotL, 2.0);
	//attenuation = pow(attenuation, 2.0);
	attenuation *= SampleShadow(index, worldPos, lightVec, lightDir);

	specularLight += CookTorrance(NdotL, NdotH, NdotV, lightColor.rgb, roughness, STANDARD, lightColor.a) * attenuation;
	//diffuseLight += (lightColor.rgb * NdotL) * (1.0 - roughness) * attenuation;
	diffuseLight += (lightColor.rgb * NdotL) * attenuation;

	//float gloss = (1.0 - roughness);
	//diffuseLight += ((lightColor.rgb * NdotL) + OrenNayarDiffuse(LdotV, NdotL, NdotV, roughness, 1.0) * gloss) * attenuation;
	//diffuseLight += lightColor.rgb * NdotL * attenuation;
	//diffuseLight += (lightColor.rgb * NdotL) * OrenNayarDiffuse(LdotV, NdotL, NdotV, roughness, 1.0) * attenuation;
}

void CalculateLighting(vec4 worldPos, vec3 normal, vec2 screenPos, out vec3 diffuseLight, out vec3 specularLight)
{
	diffuseLight = vec3(matDiffColor.rgb * ambientColor.rgb);
	specularLight = vec3(0.0);

	CalculateDirLight(worldPos, normal, diffuseLight, specularLight);

	uvec4 lightClusterData = texture(clusterTex12, CalculateClusterPos(screenPos, worldPos.w));

	while (lightClusterData.x > 0U)
	{
		CalculateLight((lightClusterData.x & 0xffU), worldPos, normal, diffuseLight, specularLight);
		lightClusterData.x >>= 8U;
	}
	while (lightClusterData.y > 0U)
	{
		CalculateLight((lightClusterData.y & 0xffU), worldPos, normal, diffuseLight, specularLight);
		lightClusterData.y >>= 8U;
	}
	while (lightClusterData.z > 0U)
	{
		CalculateLight((lightClusterData.z & 0xffU), worldPos, normal, diffuseLight, specularLight);
		lightClusterData.z >>= 8U;
	}
	while (lightClusterData.w > 0U)
	{
		CalculateLight((lightClusterData.w & 0xffU), worldPos, normal, diffuseLight, specularLight);
		lightClusterData.w >>= 8U;
	}
}

vec3 UnpackNormalMap(vec4 normalInput)
{
	vec3 normal;
	normal.xy = normalInput.ag * 2.0 - 1.0;
	normal.z = sqrt(max(1.0 - dot(normal.xy, normal.xy), 0.0));
	return normal;
}

// ====================================================================================================
void main()
{
#ifdef ALPHAMASK
	float alpha = texture(diffuseTex0, vTexCoord).a;
	if (alpha < 0.5) {
		discard;
	}
#endif

#ifdef NORMALMAP
	// vNt is the tangent space normal
	vec3 vNt = UnpackNormalMap(texture(normalTex1, vTexCoord)) * 2.0 - 1.0;
	vec3 vB = vTangent.w * cross(vNormal, vTangent.xyz);
	vec3 normal = normalize(vNt.x * vTangent.xyz + vNt.y * vB + vNt.z * vNormal);
#else
	vec3 normal = normalize(vNormal);
#endif

	vec3 diffuseLight;
	vec3 specularLight;
	CalculateLighting(vWorldPos, normal, vScreenPos, diffuseLight, specularLight);

	vec3 albedo = texture(diffuseTex0, vTexCoord).rgb;
	vec3 finalColor = albedo * diffuseLight + matSpecColor.rgb * specularLight;

	// Debug
	//finalColor = diffuseLight;
	//finalColor = specularLight;

	fragColor[0] = vec4(mix(fogColor, finalColor, GetFogFactor(vWorldPos.w)), 1.0);
	fragColor[1] = vec4(vViewNormal, 1.0);
}

#endif