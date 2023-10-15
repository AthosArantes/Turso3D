#version 330

in vec3 position;
in vec2 texCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = texCoord;
	vTexCoord.y = 1.0f - vTexCoord.y;
	gl_Position = vec4(position, 1.0);
}
