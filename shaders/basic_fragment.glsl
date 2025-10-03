#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.8, 0.8, 0.8, 1.0); // solid gray
}


// #version 330 core

// out vec4 FragColor;

// in vec3 ourColor;
// in vec2 TexCoord;

// uniform sampler2D texture1;
// uniform sampler2D texture2;

// void main()
// {
//     FragColor = mix(texture(texture1, TexCoord),
//                     texture(texture2, TexCoord),
//                     0.2);
// }