#version 330 core

#include <Uniforms.h>

#pragma shader vs //===============================================================================
#include <Transform.h>

in vec3 position;

void main()
{
	mat3x4 modelMatrix = GetWorldMatrix();
	vec3 worldPos = vec4(position, 1.0) * modelMatrix;
	gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}

#pragma shader fs //===============================================================================
out vec4 fragColor;

void main()
{
	fragColor = vec4(1.0, 1.0, 1.0, 0.5);
}
