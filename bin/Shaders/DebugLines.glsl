#version 330 core

#pragma shader:VS //===============================================================================
uniform mat4 viewProjMatrix;

in vec3 position;
in vec4 color;

out vec4 vColor;

void main()
{
	vColor = color;
	gl_Position = vec4(position, 1.0) * viewProjMatrix;
}

#pragma shader:FS //===============================================================================
in vec4 vColor;
out vec4 fragColor;

void main()
{
	fragColor = vec4(vColor.rgb, 1.0);
}
