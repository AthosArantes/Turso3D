const float PI = 3.141592653589793;

// ====================================================================================================
// Normal Distribution Functions
// ====================================================================================================
float DistributionGGX(const in float NdotH, const in float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / denom;
}

// ====================================================================================================
// Geometric Attenuation Functions
// ====================================================================================================
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

// ====================================================================================================
// Fresnel Functions
// ====================================================================================================
vec3 FresnelSchlick(const in float cosTheta, const in vec3 f0)
{
	return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(const in float cosTheta, const in vec3 f0, const in float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ====================================================================================================
// Combined Functions
// ====================================================================================================
vec3 CookTorranceBRDF(const in float NdotV, const in float NdotH, const in float NdotL, const in float HdotV, const in vec3 lightColor, const in vec3 albedo, const in vec3 f0, const in float metallic, const in float roughness)
{
	float NDF = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	vec3 F = FresnelSchlick(HdotV, f0);

	vec3 specular = (NDF * G * F) / (4.0 * NdotV * NdotL + 0.0001);
	vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

	return (kD * albedo / PI + specular) * lightColor * NdotL;
}

float OrenNayarDiffuse(const in float LdotV, const in float NdotL, const in float NdotV, const in float roughness, const in float albedo)
{
	float s = LdotV - NdotL * NdotV;
	float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));

	float sigma2 = roughness * roughness;
	float A = 1.0 + sigma2 * (albedo / (sigma2 + 0.13) + 0.5 / (sigma2 + 0.33));
	float B = 0.45 * sigma2 / (sigma2 + 0.09);

	return albedo * max(0.0, NdotL) * (A + B * s / t) / PI;
}
