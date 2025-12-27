#version 330 core

in vec2 TexCoord;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D uBaseColorTex;
uniform vec4 uBaseColorFactor;
uniform bool uHasBaseColorTex;

// Simple directional light
const vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
const vec3 ambientColor = vec3(0.3, 0.3, 0.3);
const vec3 lightColor = vec3(0.7, 0.7, 0.7);

void main()
{
    vec4 baseColor = uBaseColorFactor;

    if (uHasBaseColorTex) {
        baseColor *= texture(uBaseColorTex, TexCoord);
    }

    // Simple diffuse lighting
    float diff = max(dot(normalize(Normal), lightDir), 0.0);
    vec3 lighting = ambientColor + lightColor * diff;

    FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
