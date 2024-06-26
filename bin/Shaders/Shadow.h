#extension GL_ARB_texture_cube_map_array : enable

uniform sampler2DShadow dirShadowTex8;
uniform sampler2DShadow shadowTex9;
uniform samplerCubeArray faceSelectionTex10;
uniform usampler3D clusterTex11;

// ================================================================================================

vec3 CalculateClusterPos(const in vec2 screenPos, const in float depth)
{
	return vec3(screenPos.x, screenPos.y, sqrt(depth));
}

uvec4 GetLightClusterData(const in vec2 screenPos, const in vec4 worldPos)
{
	return texture(clusterTex11, CalculateClusterPos(screenPos, worldPos.w));
}

// ================================================================================================

float SampleShadowMap(sampler2DShadow shadowTex, const in vec4 shadowPos, const in vec4 parameters)
{
#ifdef HQSHADOW
	vec4 offsets1 = vec4(2.0 * parameters.xy * shadowPos.w, 0.0, 0.0);
	vec4 offsets2 = vec4(2.0 * parameters.x * shadowPos.w, -2.0 * parameters.y * shadowPos.w, 0.0, 0.0);
	vec4 offsets3 = vec4(2.0 * parameters.x * shadowPos.w, 0.0, 0.0, 0.0);
	vec4 offsets4 = vec4(0.0, 2.0 * parameters.y * shadowPos.w, 0.0, 0.0);

	return smoothstep(0.0, 1.0, (
		textureProjLod(shadowTex, shadowPos, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets3, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets3, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets4, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets4, 0.0)
	) * 0.1111);
#else
	vec4 offsets1 = vec4(parameters.xy * shadowPos.w, 0.0, 0.0);
	vec4 offsets2 = vec4(parameters.x * shadowPos.w, -parameters.y * shadowPos.w, 0.0, 0.0);

	return (
		textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets2, 0.0)
	) * 0.25;
#endif
}

// ================================================================================================

float SampleShadowDirectional(const in vec4 worldPos)
{
	if (dirLightShadowParameters.z < 1.0 && worldPos.w < dirLightShadowSplits.y) {
		int mat_index = worldPos.w > dirLightShadowSplits.x ? 1 : 0;

		mat4 shadowMatrix = dirLightShadowMatrices[mat_index];
		float shadowFade = dirLightShadowParameters.z + clamp((worldPos.w - dirLightShadowSplits.z) * dirLightShadowSplits.w, 0.0, 1.0);

		return clamp(shadowFade + SampleShadowMap(dirShadowTex8, vec4(worldPos.xyz, 1.0) * shadowMatrix, dirLightShadowParameters), 0.0, 1.0);
	}
	return 1.0;
}

float SampleShadow(const in vec4 worldPos, const in uint index)
{
	Light light = lights[index];

	vec3 light_dist = light.position.xyz - worldPos.xyz;

	if (light.attenuation.y > 0.0) {
		float spot_effect = dot(normalize(light_dist), light.direction.xyz);
		float attenuation = (spot_effect - light.attenuation.y) * light.attenuation.z;
		if (attenuation <= 0.0) {
			return -1.0;
		}

		if (light.shadowParameters.z < 1.0) {
			float shadow = SampleShadowMap(shadowTex9, vec4(worldPos.xyz, 1.0) * light.shadowMatrix, light.shadowParameters);
			return attenuation * clamp(light.shadowParameters.z + shadow, 0.0, 1.0);
		}

		return attenuation;

	} else if (light.shadowParameters.z < 1.0) {
		vec4 pointParameters = light.shadowMatrix[0];
		vec4 pointParameters2 = light.shadowMatrix[1];

		float zoom = pointParameters2.x;
		float q = pointParameters2.y;
		float r = pointParameters2.z;

		vec3 axis = textureLod(faceSelectionTex10, vec4(light_dist, 0.0), 0.0).xyz;
		vec4 adjust = textureLod(faceSelectionTex10, vec4(light_dist, 1.0), 0.0);

		float depth = abs(dot(light_dist, axis));
		vec3 normLightVec = (light_dist / depth) * zoom;
		vec2 coords = vec2(dot(normLightVec.zxx, axis), dot(normLightVec.yzy, axis)) * adjust.xy + adjust.zw;

		float shadow = SampleShadowMap(shadowTex9, vec4(coords * pointParameters.xy + pointParameters.zw, q + r / depth, 1.0), light.shadowParameters);

		return clamp(light.shadowParameters.z + shadow, 0.0, 1.0);
	}

	return 1.0;
}
