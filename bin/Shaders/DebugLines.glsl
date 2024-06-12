#version 330 core

#pragma shader vs //===============================================================================
uniform mat4 viewProjMatrix;

in vec3 position;
in vec4 color;

out vec4 vColor;

void main()
{
	vColor = color;
	gl_Position = vec4(position, 1.0) * viewProjMatrix;
}

#pragma shader fs //===============================================================================
in vec4 vColor;
out vec4 fragColor;

void main()
{
	fragColor = vec4(vColor.rgb, 1.0);
}
