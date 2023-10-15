uniform sampler2D ssaoTex0;
uniform vec2 blurInvSize;

in vec2 vUv;
out vec4 fragColor;

void main()
{
	const float invSamples = 1.0 / 16.0;
	const float threshold = 1.0 / 1024.0;

	float occ = texture(ssaoTex0, vUv + vec2(-0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5, -0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2(-0.5,  0.5) * blurInvSize).r;
	occ += texture(ssaoTex0, vUv + vec2( 0.5,  0.5) * blurInvSize).r;

	if (occ < threshold)
		discard;

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
