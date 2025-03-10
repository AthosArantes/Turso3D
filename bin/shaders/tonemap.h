float ColorToLuminance(const in vec3 color)
{
	return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 LinearToneMapping(const in vec3 color)
{
	return clamp(color, 0.0, 1.0);
}

vec3 ReinhardToneMapping(const in vec3 color)
{
	float luma = ColorToLuminance(color);
	float toneMappedLuma = luma / (1.0 + luma);
	if (luma > 1e-6) {
		return color * (toneMappedLuma / luma);
	}
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
	return color;
}

vec3 HableToneMapping(vec3 color)
{
	float A = 0.15; // shoulder strength
	float B = 0.50; // linear strength
	float C = 0.10; // linear angle
	float D = 0.20; // toe strength
	float E = 0.02; // toe numerator
	float F = 0.30; // toe denominator
	float W = 11.2; // linear white point
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	return color;
}

//-------------------------------------------------------------------------------------------------
//  Baking Lab
//  by MJP and David Neubelt
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//-------------------------------------------------------------------------------------------------

// The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
// credit for coming up with this fit and implementing it. Buy him a beer next time you see him. :)

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
	1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

vec3 ACESFitted(vec3 color)
{
	color *= ACESInputMat;

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color *= ACESOutputMat;

	// Clamp to [0, 1]
	color = clamp(color, 0.0, 1.0);
	return color;
}
