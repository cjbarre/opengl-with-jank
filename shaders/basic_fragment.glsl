// #version 330 core
// out vec4 FragColor;

// void main()
// {
//     FragColor = vec4(0.8, 0.8, 0.8, 1.0); // solid gray
// }


#version 330 core

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uBaseColorTex;
uniform vec4 uBaseColorFactor;
uniform bool uHasBaseColorTex; // flag in case there’s no texture

void main()
{
    vec4 baseColor = uBaseColorFactor;

    if (uHasBaseColorTex) {
        baseColor *= texture(uBaseColorTex, TexCoord);
    }

    FragColor = baseColor;
}