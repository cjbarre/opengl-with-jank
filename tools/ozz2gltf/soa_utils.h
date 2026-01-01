// SoA (Structure of Arrays) utilities for extracting individual transforms
// from ozz-animation's SIMD-packed SoaTransform structures.

#ifndef SOA_UTILS_H_
#define SOA_UTILS_H_

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/span.h"

namespace soa_utils {

// Individual joint transform (extracted from SoA format)
struct JointTransform {
    float translation[3];
    float rotation[4];  // quaternion: x, y, z, w
    float scale[3];
};

// Extract a single joint's transform from SoA rest poses.
// SoaTransform packs 4 transforms together for SIMD efficiency.
// joint_index determines which of the 4 lanes to extract.
inline JointTransform ExtractJointTransform(
    ozz::span<const ozz::math::SoaTransform> rest_poses,
    int joint_index) {

    const int soa_index = joint_index / 4;
    const int lane = joint_index % 4;
    const auto& soa = rest_poses[soa_index];

    JointTransform t;

    // Extract based on which lane (0-3) the joint is in
    switch (lane) {
        case 0:
            t.translation[0] = ozz::math::GetX(soa.translation.x);
            t.translation[1] = ozz::math::GetX(soa.translation.y);
            t.translation[2] = ozz::math::GetX(soa.translation.z);
            t.rotation[0] = ozz::math::GetX(soa.rotation.x);
            t.rotation[1] = ozz::math::GetX(soa.rotation.y);
            t.rotation[2] = ozz::math::GetX(soa.rotation.z);
            t.rotation[3] = ozz::math::GetX(soa.rotation.w);
            t.scale[0] = ozz::math::GetX(soa.scale.x);
            t.scale[1] = ozz::math::GetX(soa.scale.y);
            t.scale[2] = ozz::math::GetX(soa.scale.z);
            break;
        case 1:
            t.translation[0] = ozz::math::GetY(soa.translation.x);
            t.translation[1] = ozz::math::GetY(soa.translation.y);
            t.translation[2] = ozz::math::GetY(soa.translation.z);
            t.rotation[0] = ozz::math::GetY(soa.rotation.x);
            t.rotation[1] = ozz::math::GetY(soa.rotation.y);
            t.rotation[2] = ozz::math::GetY(soa.rotation.z);
            t.rotation[3] = ozz::math::GetY(soa.rotation.w);
            t.scale[0] = ozz::math::GetY(soa.scale.x);
            t.scale[1] = ozz::math::GetY(soa.scale.y);
            t.scale[2] = ozz::math::GetY(soa.scale.z);
            break;
        case 2:
            t.translation[0] = ozz::math::GetZ(soa.translation.x);
            t.translation[1] = ozz::math::GetZ(soa.translation.y);
            t.translation[2] = ozz::math::GetZ(soa.translation.z);
            t.rotation[0] = ozz::math::GetZ(soa.rotation.x);
            t.rotation[1] = ozz::math::GetZ(soa.rotation.y);
            t.rotation[2] = ozz::math::GetZ(soa.rotation.z);
            t.rotation[3] = ozz::math::GetZ(soa.rotation.w);
            t.scale[0] = ozz::math::GetZ(soa.scale.x);
            t.scale[1] = ozz::math::GetZ(soa.scale.y);
            t.scale[2] = ozz::math::GetZ(soa.scale.z);
            break;
        case 3:
            t.translation[0] = ozz::math::GetW(soa.translation.x);
            t.translation[1] = ozz::math::GetW(soa.translation.y);
            t.translation[2] = ozz::math::GetW(soa.translation.z);
            t.rotation[0] = ozz::math::GetW(soa.rotation.x);
            t.rotation[1] = ozz::math::GetW(soa.rotation.y);
            t.rotation[2] = ozz::math::GetW(soa.rotation.z);
            t.rotation[3] = ozz::math::GetW(soa.rotation.w);
            t.scale[0] = ozz::math::GetW(soa.scale.x);
            t.scale[1] = ozz::math::GetW(soa.scale.y);
            t.scale[2] = ozz::math::GetW(soa.scale.z);
            break;
    }

    return t;
}

}  // namespace soa_utils

#endif  // SOA_UTILS_H_
