#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

in vec2 texCoord;
out vec3 fragColor;

uniform sampler2D ssaoTex0;
uniform vec2 blurInvSize;

void main()
{
	const float invSamples = 1.0 / 16.0;
	const float threshold = 1.0 / 1024.0;

	float occ = texture(ssaoTex0, texCoord + vec2(-0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-0.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5,  0.5) * blurInvSize).r;

	if (occ < threshold) discard;

	occ += texture(ssaoTex0, texCoord + vec2(-1.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 1.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 1.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-0.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5, -1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-1.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 1.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-1.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 1.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-1.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-0.5,  1.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5,  1.5) * blurInvSize).r;
	occ *= invSamples;

	fragColor = vec3(occ);
}

#endif