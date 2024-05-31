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

uniform sampler2D sceneTex0;
uniform sampler2D sceneBlurTex1;
uniform sampler2D uiTex2;
uniform sampler2D uiMaskTex3;

void main()
{
	vec4 sceneColor = texture(sceneTex0, texCoord);
	vec4 sceneColorBlur = texture(sceneBlurTex1, texCoord);
	vec4 uiColor = texture(uiTex2, texCoord);
	float uiMask = texture(uiMaskTex3, texCoord).r;

	vec4 uiBlurredBackground = mix(sceneColor, sceneColorBlur, uiMask);

	fragColor = mix(uiBlurredBackground, uiColor, uiColor.a);
	fragColor.a = clamp((sceneColor.a + uiBlurredBackground.a), 0.0, 1.0);
}

#endif
