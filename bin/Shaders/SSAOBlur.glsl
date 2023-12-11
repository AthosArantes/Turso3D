#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec3 position;
out vec2 vUv;

void main()
{
	gl_Position = vec4(position, 1.0);
	vUv = vec2(position.xy) * 0.5 + 0.5;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

uniform sampler2D ssaoTex0;
uniform vec2 blurInvSize;

in vec2 vUv;
out vec4 fragColor;

#if 0
// Adapted: http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
vec4 GaussianBlur(int blurKernelSize, vec2 blurDir, vec2 blurRadius, float sigma, sampler2D texSampler, vec2 texCoord)
{
    int blurKernelSizeHalfSize = blurKernelSize / 2;

    // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)
    vec3 gaussCoeff;
    gaussCoeff.x = 1.0 / (sqrt(2.0 * PI) * sigma);
    gaussCoeff.y = exp(-0.5 / (sigma * sigma));
    gaussCoeff.z = gaussCoeff.y * gaussCoeff.y;

    vec2 blurVec = blurRadius * blurDir;
    vec4 avgValue = vec4(0.0);
    float gaussCoeffSum = 0.0;

    avgValue += texture2D(texSampler, texCoord) * gaussCoeff.x;
    gaussCoeffSum += gaussCoeff.x;
    gaussCoeff.xy *= gaussCoeff.yz;

    for (int i = 1; i <= blurKernelSizeHalfSize; i++)
    {
        avgValue += texture2D(texSampler, texCoord - float(i) * blurVec) * gaussCoeff.x;
        avgValue += texture2D(texSampler, texCoord + float(i) * blurVec) * gaussCoeff.x;

        gaussCoeffSum += 2.0 * gaussCoeff.x;
        gaussCoeff.xy *= gaussCoeff.yz;
    }

    return avgValue / gaussCoeffSum;
}
#endif

void main()
{
	const float invSamples = 1.0 / 16.0;
	const float threshold = 1.0 / 1024.0;

	float occ = texture(ssaoTex0, vUv + vec2(-0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-0.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5,  0.5) * blurInvSize).r;

	if (occ < threshold) discard;

	occ += texture(ssaoTex0, vUv + vec2(-1.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 1.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 1.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-0.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-1.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 1.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-1.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 1.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-1.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-0.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5,  1.5) * blurInvSize).r;
	occ *= invSamples;

	fragColor = vec4(occ, occ, occ, 1.0);
}

#endif