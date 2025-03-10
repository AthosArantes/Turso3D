vec3 BC5NormalMap(const in vec4 normalMap)
{
	vec2 n = normalMap.xy * 2.0 - 1.0;
	return vec3(n, sqrt(max(1.0 - dot(n, n), 0.0)));
}

vec3 TangentSpaceNormal(const in vec3 normal, const in vec4 vTangent, const in vec3 vBiTangent, const in vec3 vNormal)
{
	return normalize(normal.x * vTangent.xyz + normal.y * vBiTangent + normal.z * vNormal);
}

// Code adapted from https://blog.selfshadow.com/publications/blending-in-detail/
vec3 BlendNormalMap(vec3 n1, vec3 n2)
{
    vec3 t = n1 * vec3(2, 2, 2) + vec3(-1, -1, 0);
    vec3 u = n2 * vec3(-2, -2, 2) + vec3(1, 1, -1);
    return normalize(t * dot(t, u) - u * t.z);
}
