#version 330 core

out vec4 fragColor;

in vec2 vTexCoord;

uniform sampler2D diffuseTex0;
uniform float exposure;

const float GAMMA = 2.2f;

float ColorToLuminance(vec3 color)
{
	return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 LinearToneMapping(vec3 color)
{
	color = clamp(color, 0.0, 1.0);
	color = pow(color, vec3(1.0 / GAMMA));
	return color;
}

vec3 ReinhardToneMapping(vec3 color)
{
	float luma = ColorToLuminance(color);
	float toneMappedLuma = luma / (1.0 + luma);
	if (luma > 1e-6)
		color *= toneMappedLuma / luma;

	color = pow(color, vec3(1.0 / GAMMA));
	return color;
}

vec3 ACESFilmicToneMapping(vec3 color)
{
	color *= 0.6f;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
	color = pow(color, vec3(1.0 / GAMMA));
	return color;
}

vec3 HableToneMapping(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	//float exposure = 2.0;
	color *= exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	color = pow(color, vec3(1.0 / GAMMA));
	return color;
}

void main()
{
	vec3 hdr = texture(diffuseTex0, vTexCoord).rgb;

#if 0
	// reinhard
	// vec3 result = hdr / (hdr + vec3(1.0));
	// exposure
	vec3 ldr = vec3(1.0) - exp(-hdr * exposure);

	// also gamma correct while we're at it
	ldr = pow(result, vec3(1.0 / GAMMA));
#endif

	vec3 ldr = HableToneMapping(hdr);

	fragColor = vec4(ldr, 1.0);
}
