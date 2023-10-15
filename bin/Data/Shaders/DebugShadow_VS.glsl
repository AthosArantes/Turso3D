uniform mat4 worldViewProjMatrix;

in vec3 position;
in vec2 texCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = texCoord;
	gl_Position = vec4(position, 1.0) * worldViewProjMatrix;
}
