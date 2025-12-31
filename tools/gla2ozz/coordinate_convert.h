// Coordinate system conversion utilities
// JKA uses Z-up right-handed, ozz uses Y-up right-handed

#ifndef COORDINATE_CONVERT_H_
#define COORDINATE_CONVERT_H_

#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"

namespace coord_convert {

// Scale factor: JKA uses approximately 8 units per foot (~inch scale)
// Convert to meters: 1 inch = 0.0254 meters
constexpr float SCALE_FACTOR = 0.0254f;

// Convert position from Z-up right-handed to Y-up right-handed
// JKA: +X right, +Y forward, +Z up
// ozz/OpenGL: +X right, +Y up, -Z forward
// Mapping: (x, y, z) -> (x, z, -y)
inline ozz::math::Float3 ConvertPosition(float x, float y, float z) {
    return ozz::math::Float3(
        x * SCALE_FACTOR,
        z * SCALE_FACTOR,   // Z-up becomes Y
        -y * SCALE_FACTOR   // Y-forward becomes -Z
    );
}

inline ozz::math::Float3 ConvertPosition(const ozz::math::Float3& pos) {
    return ConvertPosition(pos.x, pos.y, pos.z);
}

// Convert position without scaling (for debugging/testing)
inline ozz::math::Float3 ConvertPositionNoScale(float x, float y, float z) {
    return ozz::math::Float3(x, z, -y);
}

// Convert quaternion from Z-up to Y-up coordinate system
// The coordinate change is a -90° rotation around X axis:
//   old X -> new X, old Y -> new -Z, old Z -> new Y
// For quaternion imaginary components:
//   qx -> qx (X unchanged)
//   qy -> -new_qz (old Y rotation becomes rotation around new -Z)
//   qz -> new_qy (old Z rotation becomes rotation around new Y)
// So: (qx, qy, qz, qw) -> (qx, qz, -qy, qw)
inline ozz::math::Quaternion ConvertQuaternion(float qx, float qy, float qz, float qw) {
    return ozz::math::Quaternion(qx, qz, -qy, qw);
}

inline ozz::math::Quaternion ConvertQuaternion(const ozz::math::Quaternion& q) {
    return ConvertQuaternion(q.x, q.y, q.z, q.w);
}

// Normalize a quaternion (required after decompression)
inline ozz::math::Quaternion NormalizeQuaternion(const ozz::math::Quaternion& q) {
    const float len_sq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (len_sq < 1e-10f) {
        return ozz::math::Quaternion::identity();
    }
    const float inv_len = 1.0f / std::sqrt(len_sq);
    return ozz::math::Quaternion(
        q.x * inv_len,
        q.y * inv_len,
        q.z * inv_len,
        q.w * inv_len
    );
}

}  // namespace coord_convert

#endif  // COORDINATE_CONVERT_H_
