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
in vec2 texCoord;

uniform sampler2D ssaoTex0;
uniform vec2 blurInvSize;

const float InvSamples = 1.0 / 16.0;
const float Threshold = 1.0 / 1024.0;

out vec3 fragColor;

void main()
{
	float occ = texture(ssaoTex0, texCoord + vec2(-0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2(-0.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, texCoord + vec2( 0.5,  0.5) * blurInvSize).r;

	if (occ < Threshold) discard;

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
	occ *= InvSamples;

	fragColor = vec3(occ);
}
