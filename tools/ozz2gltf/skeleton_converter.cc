// Skeleton converter implementation

#include "skeleton_converter.h"
#include "soa_utils.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace skeleton_converter {

namespace {

// Build a 4x4 matrix from TRS components
void BuildMatrix(const float* t, const float* r, const float* s, float* out) {
    // Quaternion to rotation matrix
    float qx = r[0], qy = r[1], qz = r[2], qw = r[3];

    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    // Column-major 4x4 matrix
    out[0] = s[0] * (1.0f - 2.0f * (yy + zz));
    out[1] = s[0] * (2.0f * (xy + wz));
    out[2] = s[0] * (2.0f * (xz - wy));
    out[3] = 0.0f;

    out[4] = s[1] * (2.0f * (xy - wz));
    out[5] = s[1] * (1.0f - 2.0f * (xx + zz));
    out[6] = s[1] * (2.0f * (yz + wx));
    out[7] = 0.0f;

    out[8] = s[2] * (2.0f * (xz + wy));
    out[9] = s[2] * (2.0f * (yz - wx));
    out[10] = s[2] * (1.0f - 2.0f * (xx + yy));
    out[11] = 0.0f;

    out[12] = t[0];
    out[13] = t[1];
    out[14] = t[2];
    out[15] = 1.0f;
}

// Multiply two 4x4 matrices (column-major): result = a * b
void MultiplyMatrix(const float* a, const float* b, float* result) {
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a[row + k * 4] * b[k + col * 4];
            }
            result[row + col * 4] = sum;
        }
    }
}

// Invert a 4x4 matrix (assumes affine transformation)
bool InvertMatrix(const float* m, float* out) {
    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
             m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
             m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
             m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
              m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
             m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
             m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
             m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
              m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
             m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
             m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
              m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
              m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
             m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
              m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (std::fabs(det) < 1e-10f) {
        return false;
    }

    det = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        out[i] = inv[i] * det;
    }
    return true;
}

}  // namespace

bool LoadSkeleton(const std::string& path, ozz::animation::Skeleton* skeleton) {
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened()) {
        std::cerr << "Failed to open skeleton file: " << path << std::endl;
        return false;
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Skeleton>()) {
        std::cerr << "Invalid skeleton file: " << path << std::endl;
        return false;
    }

    archive >> *skeleton;
    return true;
}

bool ConvertToGltf(const ozz::animation::Skeleton& skeleton,
                   const std::string& output_path) {
    const int num_joints = skeleton.num_joints();
    if (num_joints == 0) {
        std::cerr << "Skeleton has no joints" << std::endl;
        return false;
    }

    const auto& names = skeleton.joint_names();
    const auto& parents = skeleton.joint_parents();
    const auto& rest_poses = skeleton.joint_rest_poses();

    std::cout << "Converting skeleton with " << num_joints << " joints" << std::endl;

    // Build local transforms for each joint
    std::vector<soa_utils::JointTransform> local_transforms(num_joints);
    for (int i = 0; i < num_joints; i++) {
        local_transforms[i] = soa_utils::ExtractJointTransform(rest_poses, i);
    }

    // Compute world-space bind poses
    std::vector<float> world_matrices(num_joints * 16);
    for (int i = 0; i < num_joints; i++) {
        const auto& t = local_transforms[i];
        float local_matrix[16];
        BuildMatrix(t.translation, t.rotation, t.scale, local_matrix);

        int16_t parent = parents[i];
        if (parent == ozz::animation::Skeleton::kNoParent) {
            std::memcpy(&world_matrices[i * 16], local_matrix, sizeof(local_matrix));
        } else {
            MultiplyMatrix(&world_matrices[parent * 16], local_matrix, &world_matrices[i * 16]);
        }
    }

    // Compute inverse bind matrices
    std::vector<float> inverse_bind_matrices(num_joints * 16);
    for (int i = 0; i < num_joints; i++) {
        if (!InvertMatrix(&world_matrices[i * 16], &inverse_bind_matrices[i * 16])) {
            std::cerr << "Failed to invert bind matrix for joint " << i << std::endl;
            return false;
        }
    }

    // Build glTF model
    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "ozz2gltf";

    // Create nodes for each joint
    int root_joint = -1;
    for (int i = 0; i < num_joints; i++) {
        tinygltf::Node node;
        node.name = names[i];

        const auto& t = local_transforms[i];
        node.translation = {t.translation[0], t.translation[1], t.translation[2]};
        node.rotation = {t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3]};
        node.scale = {t.scale[0], t.scale[1], t.scale[2]};

        model.nodes.push_back(node);

        if (parents[i] == ozz::animation::Skeleton::kNoParent) {
            root_joint = i;
        }
    }

    // Build children lists
    for (int i = 0; i < num_joints; i++) {
        int16_t parent = parents[i];
        if (parent != ozz::animation::Skeleton::kNoParent) {
            model.nodes[parent].children.push_back(i);
        }
    }

    // Create buffer for inverse bind matrices
    tinygltf::Buffer buffer;
    size_t ibm_size = num_joints * 16 * sizeof(float);
    buffer.data.resize(ibm_size);
    std::memcpy(buffer.data.data(), inverse_bind_matrices.data(), ibm_size);
    model.buffers.push_back(buffer);

    // Create buffer view
    tinygltf::BufferView bufferView;
    bufferView.buffer = 0;
    bufferView.byteOffset = 0;
    bufferView.byteLength = ibm_size;
    model.bufferViews.push_back(bufferView);

    // Create accessor for inverse bind matrices
    tinygltf::Accessor accessor;
    accessor.bufferView = 0;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = num_joints;
    accessor.type = TINYGLTF_TYPE_MAT4;
    model.accessors.push_back(accessor);

    // Create skin
    tinygltf::Skin skin;
    skin.name = "skeleton";
    skin.inverseBindMatrices = 0;  // accessor index
    if (root_joint >= 0) {
        skin.skeleton = root_joint;
    }
    for (int i = 0; i < num_joints; i++) {
        skin.joints.push_back(i);
    }
    model.skins.push_back(skin);

    // Create scene with root joint(s)
    tinygltf::Scene scene;
    scene.name = "skeleton";
    for (int i = 0; i < num_joints; i++) {
        if (parents[i] == ozz::animation::Skeleton::kNoParent) {
            scene.nodes.push_back(i);
        }
    }
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    // Write output
    tinygltf::TinyGLTF writer;
    std::string err;
    std::string warn;

    bool is_binary = output_path.size() >= 4 &&
                     output_path.substr(output_path.size() - 4) == ".glb";

    bool success = writer.WriteGltfSceneToFile(&model, output_path,
                                                true,   // embed images
                                                true,   // embed buffers
                                                true,   // pretty print
                                                is_binary);

    if (!success) {
        std::cerr << "Failed to write glTF file" << std::endl;
        return false;
    }

    std::cout << "Wrote " << output_path << std::endl;
    return true;
}

}  // namespace skeleton_converter
