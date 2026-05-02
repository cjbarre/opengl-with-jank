#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uFontTexture;
uniform vec3 uTextColor;

void main()
{
    float alpha = texture(uFontTexture, TexCoord).r;
    FragColor = vec4(uTextColor, alpha);
}
