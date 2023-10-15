in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D diffuseTex0;

void main()
{
	float depth = texture(diffuseTex0, vTexCoord).r;
	// Raise to 2nd power to see differences better
	depth *= depth;
	fragColor = vec4(depth, depth, depth, 1.0);
}
