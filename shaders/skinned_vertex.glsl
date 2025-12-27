#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec4 aJoints;   // bone indices
layout (location = 4) in vec4 aWeights;   // bone weights

// Maximum number of bones supported (Ruby model needs up to 190)
#define MAX_BONES 200

uniform mat4 uBoneMatrices[MAX_BONES];
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
out vec3 Normal;

void main()
{
    // Compute skinning matrix from bone influences
    mat4 skinMatrix =
        aWeights.x * uBoneMatrices[aJoints.x] +
        aWeights.y * uBoneMatrices[aJoints.y] +
        aWeights.z * uBoneMatrices[aJoints.z] +
        aWeights.w * uBoneMatrices[aJoints.w];

    // Apply skinning to position
    vec4 skinnedPos = skinMatrix * vec4(aPos, 1.0);

    // Apply skinning to normal (using mat3 to ignore translation)
    vec3 skinnedNormal = mat3(skinMatrix) * aNormal;

    // Transform to clip space
    gl_Position = projection * view * model * skinnedPos;

    // Pass interpolated values to fragment shader
    TexCoord = aTexCoord;
    Normal = normalize(mat3(model) * skinnedNormal);
}
