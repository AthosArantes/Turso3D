#version 330 core

#pragma shader vs //===============================================================================
in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#pragma shader fs //===============================================================================
in vec2 texCoord;

uniform sampler2D sceneTex0;
uniform sampler2D sceneBlurTex1;
uniform sampler2D uiTex2;
uniform sampler2D uiMaskTex3;

out vec4 fragColor;

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
