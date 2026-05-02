#version 330 core
out vec4 FragColor;
uniform vec3 lineColor;
uniform vec3 lineColor2;
in float edgeDist;
in float boneHeight;
void main() {
    // Gradient based on bone height (feet to head)
    float t = clamp(boneHeight * 0.5 + 0.5, 0.0, 1.0);
    vec3 color = mix(lineColor, lineColor2, t);

    // Soft edge falloff for anti-aliasing
    float alpha = 1.0 - smoothstep(0.7, 1.0, abs(edgeDist));

    // Slight glow at center
    float glow = 1.0 + 0.3 * (1.0 - abs(edgeDist));
    FragColor = vec4(color * glow, alpha);
}
