#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;
uniform mat4 projection;
uniform float lineWidth;
in vec3 worldPos[];
out float edgeDist;
out float boneHeight;
void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    // Calculate line direction in screen space
    vec2 dir = normalize(p1.xy/p1.w - p0.xy/p0.w);
    vec2 normal = vec2(-dir.y, dir.x) * lineWidth * 0.5;

    // Pass bone height for gradient (use average Y)
    float h = (worldPos[0].y + worldPos[1].y) * 0.5;

    // Emit quad vertices
    edgeDist = -1.0; boneHeight = h;
    gl_Position = projection * vec4(p0.xy + normal * p0.w, p0.z, p0.w);
    EmitVertex();

    edgeDist = 1.0; boneHeight = h;
    gl_Position = projection * vec4(p0.xy - normal * p0.w, p0.z, p0.w);
    EmitVertex();

    edgeDist = -1.0; boneHeight = h;
    gl_Position = projection * vec4(p1.xy + normal * p1.w, p1.z, p1.w);
    EmitVertex();

    edgeDist = 1.0; boneHeight = h;
    gl_Position = projection * vec4(p1.xy - normal * p1.w, p1.z, p1.w);
    EmitVertex();

    EndPrimitive();
}
