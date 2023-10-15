in vec3 position;
out vec2 vUv;

void main()
{
	gl_Position = vec4(position, 1.0);
	vUv = vec2(position.xy) * 0.5 + 0.5;
}
