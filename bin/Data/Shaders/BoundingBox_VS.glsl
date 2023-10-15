#include "Uniforms.glsli"
#include "Transform.glsli"

in vec3 position;

void main()
{
	mat3x4 modelMatrix = GetWorldMatrix();
	vec3 worldPos = vec4(position, 1.0) * modelMatrix;
	gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}
