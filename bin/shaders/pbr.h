#include <shadow.h>

uniform samplerCube iemTex12; // Irradiance Environment Map
uniform samplerCube pmremTex13; // Prefiltered Mipmaped Radiance Environment Map
uniform sampler2D brdfTex14; // BRDF LUT map

// ================================================================================================

const float PI = 3.141592653589793;
const float EPSILON = 0.0001;
const float IOR = 1.5;

float DistributionGGX(const in float NdotH, const in float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / denom;
}

float GeometrySchlickGGX(const in float NdotV, const in float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(const in float NdotV, const in float NdotL, const in float roughness)
{
	return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(const in float cosTheta, const in vec3 f0)
{
	return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(const in float cosTheta, const in vec3 f0, const in float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CookTorrance(const in vec3 V, const in vec3 N, const in vec3 L, const in vec3 lightColor, const in vec3 albedo, const in vec3 f0, const in float metallic, const in float roughness)
{
	vec3 H = normalize(L + V);
	float NdotV = max(0.0, dot(N, V));
	float NdotL = max(0.0, dot(N, L));
	float NdotH = max(0.0, dot(N, H));
	float HdotV = max(0.0, dot(H, V));

	vec3 F = FresnelSchlick(HdotV, f0);
	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);

	vec3 specular = (F * D * G) / (PI * NdotV * NdotL + EPSILON);

	vec3 kD = (1.0 - F) * (1.0 - metallic);
	return (albedo * kD / PI + specular) * lightColor * NdotL;
}

vec3 CalcLight(const uint index, const in vec4 worldPos, const in vec3 normal, const in vec3 albedo, const in vec3 f0, const in float metallic, const in float roughness)
{
	Light light = lights[index];

#ifdef LIGHTMASK
	if ((light.viewMask & lightMask) == 0u) {
		return vec3(0.0);
	}
#endif

	vec3 V = normalize(cameraPosition.xyz - worldPos.xyz);
	vec3 D = light.position.xyz - worldPos.xyz;
	vec3 L = normalize(D);

	vec3 sd = D * light.attenuation.x;
	float attenuation = max(0.0, 1.0 - dot(sd, sd));
	if (attenuation <= 0.0) {
		return vec3(0.0);
	}

	float shadow = SampleShadow(worldPos, index);
	if (shadow <= 0.0) {
		return vec3(0.0);
	}

	return CookTorrance(V, normal, L, light.color.rgb, albedo, f0, metallic, roughness) * attenuation * shadow;
}

// ================================================================================================

vec3 CalculateLighting(const in vec4 worldPos, const in vec2 screenPos, const in vec3 normal, const in vec3 albedo, const in float metallic, const in float roughness)
{
	vec3 V = normalize(cameraPosition.xyz - worldPos.xyz);

	// Calculate color at normal incidence
	vec3 f0 = vec3(abs((1.0 - IOR) / (1.0 + IOR)));
	f0 = mix(f0 * f0, albedo, metallic);

	// Ambient lighting
	vec3 ambient = albedo * ambientColor.rgb;
	{
		float NdotV = max(0.0, dot(normal, V));

		vec3 F = FresnelSchlickRoughness(NdotV, f0, roughness);
		vec3 R = reflect(-V, normal);

		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		vec3 irradiance = texture(iemTex12, normal).rgb;
		vec3 diffuse = irradiance * albedo * kD;

		// Sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
		vec3 specIrradiance = textureLod(pmremTex13, R, roughness * iblParameters.x).rgb;
		vec2 specBRDF = texture(brdfTex14, vec2(NdotV, roughness)).rg;
		vec3 specularIBL = (F * specBRDF.x + specBRDF.y) * specIrradiance;

		ambient += diffuse + specularIBL;
	}

	// Directional light
	vec3 color = CookTorrance(V, normal, normalize(dirLightDirection.xyz), dirLightColor.rgb, albedo, f0, metallic, roughness) * SampleShadowDirectional(worldPos);

	// Point/Spot lights
	uvec4 cluster = GetLightClusterData(screenPos, worldPos);
	while (cluster.x > 0u) {
		uint index = (cluster.x & 0xffu);
		color += CalcLight(index, worldPos, normal, albedo, f0, metallic, roughness);
		cluster.x >>= 8u;
	}
	while (cluster.y > 0u) {
		uint index = (cluster.y & 0xffu);
		color += CalcLight(index, worldPos, normal, albedo, f0, metallic, roughness);
		cluster.y >>= 8u;
	}
	while (cluster.z > 0u) {
		uint index = (cluster.z & 0xffu);
		color += CalcLight(index, worldPos, normal, albedo, f0, metallic, roughness);
		cluster.z >>= 8u;
	}
	while (cluster.w > 0u) {
		uint index = (cluster.w & 0xffu);
		color += CalcLight(index, worldPos, normal, albedo, f0, metallic, roughness);
		cluster.w >>= 8u;
	}

	return ambient + color;
}
