#include "Uniforms.glsli"
#include "Transform.glsli"

in vec3 position;
in vec3 normal;

out vec4 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
noperspective out vec2 vScreenPos;

void main()
{
	mat3x4 world = GetWorldMatrix();

	vWorldPos.xyz = vec4(position, 1.0) * world;
	vNormal = normalize((vec4(normal, 0.0) * world));
	vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;
	gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
	vWorldPos.w = CalculateDepth(gl_Position);
	vScreenPos = CalculateScreenPos(gl_Position);
}
