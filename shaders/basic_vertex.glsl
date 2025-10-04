// #version 330 core
// layout (location = 0) in vec3 aPos;

// uniform mat4 model;
// uniform mat4 view;
// uniform mat4 projection;

// void main()
// {
//     gl_Position = projection * view * model * vec4(aPos, 1.0);
// }

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;   // optional for lighting
layout (location = 2) in vec2 aTexCoord; // glTF TEXCOORD_0

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord; // pass UVs to fragment shader
}