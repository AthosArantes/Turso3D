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

in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D srcTex0; // ui
uniform sampler2D sceneTex1; // scene
uniform sampler2D sceneBlurTex2; // blurred scene

void main()
{
	vec4 samplerUi = texture(srcTex0, texCoord);
	vec4 samplerScene = texture(sceneTex1, texCoord);
	vec4 samplerSceneBlur = texture(sceneBlurTex2, texCoord);

	float alpha = samplerUi.a;
	float mask = 1.0 - step(0.0, -alpha);

	vec4 gui = mix(samplerSceneBlur, samplerUi, alpha);
	fragColor = mix(samplerScene, gui, mask);
	fragColor.a = clamp((samplerScene.a + gui.a), 0.0, 1.0);
}

#endif
