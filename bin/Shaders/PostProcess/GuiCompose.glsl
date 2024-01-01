#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

uniform sampler2D srcTex0; // ui
uniform sampler2D sceneTex1; // scene
uniform sampler2D sceneBlurTex2; // blurred scene

in vec2 texCoord;
out vec4 fragColor;

void main()
{
	vec4 samplerUi = texture(srcTex0, texCoord);
	vec4 samplerScene = texture(sceneTex1, texCoord);
	vec4 samplerSceneBlur = texture(sceneBlurTex2, texCoord);

	float alpha = samplerUi.a;

	fragColor = samplerScene;
	if (alpha > 0.0) {
		fragColor = mix(samplerSceneBlur, samplerUi, alpha);
	}
}

#endif
