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
// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!

in vec2 texCoord;

uniform sampler2D srcTex0;

uniform float filterRadius;
uniform float aspectRatio;

const float WeightMul = 1.0 / 16.0;

out vec4 upsample;

void main()
{
	// The filter kernel is applied with a radius, specified in texture
	// coordinates, so that the radius will vary across mip resolutions.
	float x = filterRadius;
	float y = filterRadius * aspectRatio;

	// Take 9 samples around current texel:
	// a - b - c
	// d - e - f
	// g - h - i
	// === ('e' is the current texel) ===
	vec4 a = texture(srcTex0, vec2(texCoord.x - x, texCoord.y + y));
	vec4 b = texture(srcTex0, vec2(texCoord.x,     texCoord.y + y));
	vec4 c = texture(srcTex0, vec2(texCoord.x + x, texCoord.y + y));

	vec4 d = texture(srcTex0, vec2(texCoord.x - x, texCoord.y));
	vec4 e = texture(srcTex0, vec2(texCoord.x,     texCoord.y));
	vec4 f = texture(srcTex0, vec2(texCoord.x + x, texCoord.y));

	vec4 g = texture(srcTex0, vec2(texCoord.x - x, texCoord.y - y));
	vec4 h = texture(srcTex0, vec2(texCoord.x,     texCoord.y - y));
	vec4 i = texture(srcTex0, vec2(texCoord.x + x, texCoord.y - y));

	// Apply weighted distribution, by using a 3x3 tent filter:
	//  1   | 1 2 1 |
	// -- * | 2 4 2 |
	// 16   | 1 2 1 |
	upsample = e*4.0;
	upsample += (b+d+f+h)*2.0;
	upsample += (a+c+g+i);
	upsample *= WeightMul;
}
