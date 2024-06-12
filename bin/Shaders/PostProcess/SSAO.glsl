#version 330 core

#pragma shader vs //===============================================================================
in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#pragma shader fs //===============================================================================
#include <Uniforms.h>

in vec2 texCoord;

uniform sampler2D depthTex0;
uniform sampler2D normalTex1;
uniform sampler2D noiseTex2;

layout(std140) uniform SSAOData4
{
	vec2 noiseInvSize;
	vec2 screenInvSize;
	vec4 frustumSize;
	vec2 depthReconstruct;

	vec4 aoParameters;
};

// ------------------------------------------------------------------------------------------------
float GetLinearDepth(const in vec2 uv)
{
	return depthReconstruct.y / (texture(depthTex0, uv).r - depthReconstruct.x);
}
vec3 GetPosition(const in float depth, const in vec2 uv)
{
	return vec3((uv - 0.5) * frustumSize.xy, frustumSize.z) * depth;
}
vec3 GetPosition(const in vec2 uv)
{
	return GetPosition(GetLinearDepth(uv), uv);
}

float DoAmbientOcclusion(vec2 uv, vec2 offs, vec3 pos, vec3 n)
{
	offs.x *= frustumSize.w;
	vec3 diff = GetPosition(uv + offs) - pos;
	vec3 v = normalize(diff);
	return dot(v, n) * clamp(1.0 - abs(diff.z) * aoParameters.y, 0.0, 1.0);
}

// ------------------------------------------------------------------------------------------------
out float fragColor;

void main()
{
	vec2 rand = texture(noiseTex2, texCoord * noiseInvSize.xy).rg * 2.0 - 1.0;
	vec3 pos = GetPosition(texCoord);

	float rad = min(aoParameters.x / pos.z, aoParameters.w);
	float ao = 0.0;

	if (rad > 3.5 * screenInvSize.x)
	{
		vec3 normal = texture(normalTex1, texCoord).rgb * 2.0 - 1.0;

		vec2 vec[4] = vec2[](
			vec2(1,0),
			vec2(-1,0),
			vec2(0,1),
			vec2(0,-1)
		);

		const int iterations = 4;
		for (int i = 0; i < iterations; ++i)
		{
			vec2 coord1 = reflect(vec[i], rand) * rad;
			vec2 coord2 = vec2(coord1.x * 0.707 - coord1.y * 0.707, coord1.x * 0.707 + coord1.y * 0.707);

			ao += DoAmbientOcclusion(texCoord, coord1 * 0.5, pos, normal);
			ao += DoAmbientOcclusion(texCoord, coord2, pos, normal);
		}
	}

	fragColor = clamp(ao * aoParameters.z, 0.0, 1.0);
}
