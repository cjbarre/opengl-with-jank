#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
out vec3 worldPos;
void main() {
    vec4 wp = model * vec4(aPos, 1.0);
    worldPos = wp.xyz;
    gl_Position = view * wp;
}
