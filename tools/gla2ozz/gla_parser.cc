// GLA file parser implementation

#include "gla_parser.h"

#include <cmath>
#include <cstring>
#include <fstream>

#include "ozz/base/log.h"

namespace gla {

// Quaternion helper functions
namespace {

// Quaternion conjugate (inverse for unit quaternion)
ozz::math::Quaternion QuaternionConjugate(const ozz::math::Quaternion& q) {
    return ozz::math::Quaternion(-q.x, -q.y, -q.z, q.w);
}

// Quaternion multiplication: a * b
// (a.w + a.x*i + a.y*j + a.z*k) * (b.w + b.x*i + b.y*j + b.z*k)
ozz::math::Quaternion QuaternionMultiply(const ozz::math::Quaternion& a,
                                          const ozz::math::Quaternion& b) {
    return ozz::math::Quaternion(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,  // x
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,  // y
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,  // z
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z   // w
    );
}

// Normalize quaternion
ozz::math::Quaternion QuaternionNormalize(const ozz::math::Quaternion& q) {
    float len_sq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (len_sq < 1e-10f) {
        return ozz::math::Quaternion::identity();
    }
    float inv_len = 1.0f / std::sqrt(len_sq);
    return ozz::math::Quaternion(q.x * inv_len, q.y * inv_len, q.z * inv_len, q.w * inv_len);
}

// Rotate vector by quaternion: q * v * q^-1
// For unit quaternion, q^-1 = conjugate(q)
ozz::math::Float3 RotateVectorByQuaternion(const ozz::math::Quaternion& q,
                                            const ozz::math::Float3& v) {
    // Convert vector to quaternion (w=0)
    ozz::math::Quaternion qv(v.x, v.y, v.z, 0.0f);

    // result = q * v * q^-1
    ozz::math::Quaternion q_conj = QuaternionConjugate(q);
    ozz::math::Quaternion temp = QuaternionMultiply(q, qv);
    ozz::math::Quaternion result = QuaternionMultiply(temp, q_conj);

    return ozz::math::Float3(result.x, result.y, result.z);
}

// Build 3x4 matrix from quaternion and translation
void BuildMatrix34(const ozz::math::Quaternion& q, const ozz::math::Float3& t,
                   float out[3][4]) {
    // Convert quaternion to rotation matrix (same as MC_UnCompressQuat)
    float fTx = 2.0f * q.x;
    float fTy = 2.0f * q.y;
    float fTz = 2.0f * q.z;
    float fTwx = fTx * q.w;
    float fTwy = fTy * q.w;
    float fTwz = fTz * q.w;
    float fTxx = fTx * q.x;
    float fTxy = fTy * q.x;
    float fTxz = fTz * q.x;
    float fTyy = fTy * q.y;
    float fTyz = fTz * q.y;
    float fTzz = fTz * q.z;

    out[0][0] = 1.0f - (fTyy + fTzz);
    out[0][1] = fTxy - fTwz;
    out[0][2] = fTxz + fTwy;
    out[1][0] = fTxy + fTwz;
    out[1][1] = 1.0f - (fTxx + fTzz);
    out[1][2] = fTyz - fTwx;
    out[2][0] = fTxz - fTwy;
    out[2][1] = fTyz + fTwx;
    out[2][2] = 1.0f - (fTxx + fTyy);

    out[0][3] = t.x;
    out[1][3] = t.y;
    out[2][3] = t.z;
}

// Multiply two 3x4 matrices: C = A * B
// Treats as 4x4 with implicit [0,0,0,1] bottom row
void MultiplyMatrix34(const float a[3][4], const float b[3][4], float c[3][4]) {
    for (int i = 0; i < 3; i++) {
        c[i][0] = a[i][0] * b[0][0] + a[i][1] * b[1][0] + a[i][2] * b[2][0];
        c[i][1] = a[i][0] * b[0][1] + a[i][1] * b[1][1] + a[i][2] * b[2][1];
        c[i][2] = a[i][0] * b[0][2] + a[i][1] * b[1][2] + a[i][2] * b[2][2];
        c[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3];
    }
}

// Set matrix to identity
void SetIdentityMatrix34(float m[3][4]) {
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
}

}  // anonymous namespace

GlaParser::GlaParser()
    : m_frame_data(nullptr),
      m_bone_pool(nullptr),
      m_frame_data_size(0),
      m_bone_pool_size(0) {
    memset(&m_header, 0, sizeof(m_header));
}

GlaParser::~GlaParser() {
}

bool GlaParser::Load(const char* filename) {
    // Read entire file into memory
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ozz::log::Err() << "Failed to open GLA file: " << filename << std::endl;
        return false;
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    m_file_data.resize(file_size);
    if (!file.read(reinterpret_cast<char*>(m_file_data.data()), file_size)) {
        ozz::log::Err() << "Failed to read GLA file: " << filename << std::endl;
        return false;
    }
    file.close();

    // Parse header
    if (!ParseHeader()) {
        return false;
    }

    // Parse skeleton
    if (!ParseSkeleton()) {
        return false;
    }

    // Set up pointers to frame data and bone pool
    if (!LoadFrameIndices()) {
        return false;
    }

    if (!LoadCompressedBonePool()) {
        return false;
    }

    ozz::log::LogV() << "Loaded GLA: " << m_header.num_bones << " bones, "
                     << m_header.num_frames << " frames" << std::endl;

    return true;
}

bool GlaParser::ParseHeader() {
    if (m_file_data.size() < sizeof(GlaHeader)) {
        ozz::log::Err() << "File too small to contain GLA header" << std::endl;
        return false;
    }

    memcpy(&m_header, m_file_data.data(), sizeof(GlaHeader));

    // Verify magic
    if (m_header.ident != GLA_IDENT) {
        ozz::log::Err() << "Invalid GLA magic. Expected 0x"
                        << std::hex << GLA_IDENT
                        << " got 0x" << m_header.ident << std::dec << std::endl;
        return false;
    }

    // Verify version
    if (m_header.version != GLA_VERSION) {
        ozz::log::Err() << "Unsupported GLA version. Expected "
                        << GLA_VERSION << " got " << m_header.version << std::endl;
        return false;
    }

    ozz::log::LogV() << "GLA header: name='" << m_header.name << "'"
                     << " scale=" << m_header.scale
                     << " frames=" << m_header.num_frames
                     << " bones=" << m_header.num_bones << std::endl;

    return true;
}

bool GlaParser::ParseSkeleton() {
    if (m_header.ofs_skel <= 0 ||
        static_cast<size_t>(m_header.ofs_skel) >= m_file_data.size()) {
        ozz::log::Err() << "Invalid skeleton offset" << std::endl;
        return false;
    }

    m_bones.clear();
    m_bones.reserve(m_header.num_bones);

    // The bone offset table is located immediately after the header (at sizeof(GlaHeader))
    // Each offset is relative to sizeof(GlaHeader), pointing to the actual bone data
    const size_t header_size = sizeof(GlaHeader);
    const uint8_t* file_base = m_file_data.data();

    // Verify we have space for the offset table
    size_t offset_table_size = static_cast<size_t>(m_header.num_bones) * sizeof(int32_t);
    if (header_size + offset_table_size > m_file_data.size()) {
        ozz::log::Err() << "File too small for bone offset table" << std::endl;
        return false;
    }

    const int32_t* bone_offsets = reinterpret_cast<const int32_t*>(file_base + header_size);

    for (int i = 0; i < m_header.num_bones; ++i) {
        // Offset is relative to sizeof(GlaHeader)
        int32_t bone_offset = bone_offsets[i];
        const uint8_t* bone_data = file_base + header_size + bone_offset;

        // Verify we don't read past end of file
        if (bone_data + sizeof(GlaSkelBoneHeader) > file_base + m_file_data.size()) {
            ozz::log::Err() << "Bone " << i << " (offset=" << bone_offset
                           << ") data extends past end of file" << std::endl;
            return false;
        }

        GlaBone bone;
        const GlaSkelBoneHeader* header = reinterpret_cast<const GlaSkelBoneHeader*>(bone_data);

        memcpy(bone.name, header->name, MAX_BONE_NAME);
        bone.name[MAX_BONE_NAME - 1] = '\0';  // Ensure null termination
        bone.flags = header->flags;
        bone.parent = header->parent;
        bone.base_pose = header->base_pose;
        bone.base_pose_inv = header->base_pose_inv;
        bone.num_children = header->num_children;

        m_bones.push_back(bone);

        ozz::log::LogV() << "  Bone " << i << ": '" << bone.name << "'"
                         << " parent=" << bone.parent
                         << " children=" << bone.num_children << std::endl;
    }

    return true;
}

bool GlaParser::LoadFrameIndices() {
    if (m_header.ofs_frames <= 0 ||
        static_cast<size_t>(m_header.ofs_frames) >= m_file_data.size()) {
        ozz::log::Err() << "Invalid frame data offset" << std::endl;
        return false;
    }

    m_frame_data = m_file_data.data() + m_header.ofs_frames;

    // Each frame has num_bones * 3 bytes (3-byte indices)
    // Total size = num_frames * num_bones * 3, padded to 4-byte boundary
    size_t frame_entry_size = static_cast<size_t>(m_header.num_bones) * 3;
    m_frame_data_size = static_cast<size_t>(m_header.num_frames) * frame_entry_size;

    // Verify we have enough data
    if (m_frame_data + m_frame_data_size > m_file_data.data() + m_file_data.size()) {
        ozz::log::Err() << "Frame data extends past end of file" << std::endl;
        return false;
    }

    return true;
}

bool GlaParser::LoadCompressedBonePool() {
    if (m_header.ofs_comp_bone_pool <= 0 ||
        static_cast<size_t>(m_header.ofs_comp_bone_pool) >= m_file_data.size()) {
        ozz::log::Err() << "Invalid compressed bone pool offset" << std::endl;
        return false;
    }

    m_bone_pool = m_file_data.data() + m_header.ofs_comp_bone_pool;

    // Pool extends to the skeleton section (or end of file)
    size_t pool_end = m_header.ofs_skel > 0 ?
        static_cast<size_t>(m_header.ofs_skel) :
        m_file_data.size();
    m_bone_pool_size = pool_end - static_cast<size_t>(m_header.ofs_comp_bone_pool);

    return true;
}

uint32_t GlaParser::ReadFrameIndex(int frame, int bone) const {
    if (frame < 0 || frame >= m_header.num_frames ||
        bone < 0 || bone >= m_header.num_bones) {
        return 0;
    }

    // Each frame stores num_bones 3-byte indices
    size_t offset = static_cast<size_t>(frame) * static_cast<size_t>(m_header.num_bones) * 3 +
                    static_cast<size_t>(bone) * 3;

    if (offset + 3 > m_frame_data_size) {
        return 0;
    }

    // Read 3-byte little-endian index
    const uint8_t* p = m_frame_data + offset;
    uint32_t idx = static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16);

    return idx;
}

void GlaParser::DecompressBone(const CompressedBone& comp,
                                ozz::math::Float3* translation,
                                ozz::math::Quaternion* rotation) const {
    // The compressed bone format from JK2/JKA SDK (matcomp.h)
    // 14 bytes total, interpreted as 7 unsigned shorts
    const uint8_t* d = comp.data;

    // Read 7 unsigned shorts (little-endian)
    auto read_u16 = [](const uint8_t* p) -> uint16_t {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    };

    // Bytes 0-7: Quaternion (4 components as unsigned shorts)
    // OpenJK MC_UnCompressQuat uses order: w, x, y, z (scalar-first)
    // Each component maps from [0, 32767] to [-2, 2] approximately
    uint16_t qw_raw = read_u16(d + 0);  // W is first!
    uint16_t qx_raw = read_u16(d + 2);
    uint16_t qy_raw = read_u16(d + 4);
    uint16_t qz_raw = read_u16(d + 6);

    // Convert to float - exact formula from JK2 SDK matcomp.cpp MC_UnCompress
    // The SDK uses a range of [-2, 2] for quaternion components before normalization
    float qx = (static_cast<float>(qx_raw) / 16383.0f) - 2.0f;
    float qy = (static_cast<float>(qy_raw) / 16383.0f) - 2.0f;
    float qz = (static_cast<float>(qz_raw) / 16383.0f) - 2.0f;
    float qw = (static_cast<float>(qw_raw) / 16383.0f) - 2.0f;

    // Normalize the quaternion
    float len_sq = qx * qx + qy * qy + qz * qz + qw * qw;
    if (len_sq > 1e-10f) {
        float inv_len = 1.0f / std::sqrt(len_sq);
        qx *= inv_len;
        qy *= inv_len;
        qz *= inv_len;
        qw *= inv_len;
    } else {
        qx = 0.0f;
        qy = 0.0f;
        qz = 0.0f;
        qw = 1.0f;
    }

    *rotation = ozz::math::Quaternion(qx, qy, qz, qw);

    // Bytes 8-13: Translation (3 components as unsigned shorts)
    // Maps from [0, 65535] to [-512, 512] in JKA units
    uint16_t tx_raw = read_u16(d + 8);
    uint16_t ty_raw = read_u16(d + 10);
    uint16_t tz_raw = read_u16(d + 12);

    float tx = (static_cast<float>(tx_raw) / 64.0f) - 512.0f;
    float ty = (static_cast<float>(ty_raw) / 64.0f) - 512.0f;
    float tz = (static_cast<float>(tz_raw) / 64.0f) - 512.0f;

    *translation = ozz::math::Float3(tx, ty, tz);
}

bool GlaParser::GetBoneTransform(int frame_index, int bone_index,
                                  ozz::math::Float3* translation,
                                  ozz::math::Quaternion* rotation) const {
    if (frame_index < 0 || frame_index >= m_header.num_frames ||
        bone_index < 0 || bone_index >= m_header.num_bones) {
        return false;
    }

    // Get the pool index for this frame/bone
    uint32_t pool_index = ReadFrameIndex(frame_index, bone_index);

    // Calculate offset into bone pool (14 bytes per compressed bone)
    size_t pool_offset = static_cast<size_t>(pool_index) * 14;
    if (pool_offset + 14 > m_bone_pool_size) {
        ozz::log::Err() << "Bone pool index " << pool_index
                        << " out of bounds for frame " << frame_index
                        << " bone " << bone_index << std::endl;
        return false;
    }

    const CompressedBone* comp = reinterpret_cast<const CompressedBone*>(
        m_bone_pool + pool_offset);

    DecompressBone(*comp, translation, rotation);
    return true;
}

bool GlaParser::DecomposeMatrix(const GlaMatrix34& matrix,
                                 ozz::math::Float3* translation,
                                 ozz::math::Quaternion* rotation,
                                 ozz::math::Float3* scale) const {
    // Extract translation from last column
    translation->x = matrix.matrix[0][3];
    translation->y = matrix.matrix[1][3];
    translation->z = matrix.matrix[2][3];

    // Extract scale by computing column magnitudes
    float sx = std::sqrt(matrix.matrix[0][0] * matrix.matrix[0][0] +
                         matrix.matrix[1][0] * matrix.matrix[1][0] +
                         matrix.matrix[2][0] * matrix.matrix[2][0]);
    float sy = std::sqrt(matrix.matrix[0][1] * matrix.matrix[0][1] +
                         matrix.matrix[1][1] * matrix.matrix[1][1] +
                         matrix.matrix[2][1] * matrix.matrix[2][1]);
    float sz = std::sqrt(matrix.matrix[0][2] * matrix.matrix[0][2] +
                         matrix.matrix[1][2] * matrix.matrix[1][2] +
                         matrix.matrix[2][2] * matrix.matrix[2][2]);

    *scale = ozz::math::Float3(sx, sy, sz);

    // Build normalized rotation matrix
    float m[3][3];
    if (sx > 1e-6f) {
        m[0][0] = matrix.matrix[0][0] / sx;
        m[1][0] = matrix.matrix[1][0] / sx;
        m[2][0] = matrix.matrix[2][0] / sx;
    } else {
        m[0][0] = 1.0f; m[1][0] = 0.0f; m[2][0] = 0.0f;
    }

    if (sy > 1e-6f) {
        m[0][1] = matrix.matrix[0][1] / sy;
        m[1][1] = matrix.matrix[1][1] / sy;
        m[2][1] = matrix.matrix[2][1] / sy;
    } else {
        m[0][1] = 0.0f; m[1][1] = 1.0f; m[2][1] = 0.0f;
    }

    if (sz > 1e-6f) {
        m[0][2] = matrix.matrix[0][2] / sz;
        m[1][2] = matrix.matrix[1][2] / sz;
        m[2][2] = matrix.matrix[2][2] / sz;
    } else {
        m[0][2] = 0.0f; m[1][2] = 0.0f; m[2][2] = 1.0f;
    }

    // Convert rotation matrix to quaternion
    float trace = m[0][0] + m[1][1] + m[2][2];
    float qw, qx, qy, qz;

    if (trace > 0.0f) {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        qw = 0.25f / s;
        qx = (m[2][1] - m[1][2]) * s;
        qy = (m[0][2] - m[2][0]) * s;
        qz = (m[1][0] - m[0][1]) * s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        float s = 2.0f * std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]);
        qw = (m[2][1] - m[1][2]) / s;
        qx = 0.25f * s;
        qy = (m[0][1] + m[1][0]) / s;
        qz = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        float s = 2.0f * std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]);
        qw = (m[0][2] - m[2][0]) / s;
        qx = (m[0][1] + m[1][0]) / s;
        qy = 0.25f * s;
        qz = (m[1][2] + m[2][1]) / s;
    } else {
        float s = 2.0f * std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]);
        qw = (m[1][0] - m[0][1]) / s;
        qx = (m[0][2] + m[2][0]) / s;
        qy = (m[1][2] + m[2][1]) / s;
        qz = 0.25f * s;
    }

    *rotation = ozz::math::Quaternion(qx, qy, qz, qw);
    return true;
}

bool GlaParser::GetBindPoseTransform(int bone_index,
                                      ozz::math::Float3* translation,
                                      ozz::math::Quaternion* rotation,
                                      ozz::math::Float3* scale) const {
    if (bone_index < 0 || static_cast<size_t>(bone_index) >= m_bones.size()) {
        return false;
    }

    return DecomposeMatrix(m_bones[bone_index].base_pose,
                           translation, rotation, scale);
}

void GlaParser::InvertMatrix(const GlaMatrix34& m, GlaMatrix34* result) const {
    // For a 3x4 affine matrix with possible uniform scale, invert properly
    // The GLA matrices have scale factor (0.64) baked into the rotation columns

    // Compute scale from column magnitudes
    float sx = std::sqrt(m.matrix[0][0] * m.matrix[0][0] +
                         m.matrix[1][0] * m.matrix[1][0] +
                         m.matrix[2][0] * m.matrix[2][0]);
    float sy = std::sqrt(m.matrix[0][1] * m.matrix[0][1] +
                         m.matrix[1][1] * m.matrix[1][1] +
                         m.matrix[2][1] * m.matrix[2][1]);
    float sz = std::sqrt(m.matrix[0][2] * m.matrix[0][2] +
                         m.matrix[1][2] * m.matrix[1][2] +
                         m.matrix[2][2] * m.matrix[2][2]);

    // Avoid division by zero
    if (sx < 1e-6f) sx = 1.0f;
    if (sy < 1e-6f) sy = 1.0f;
    if (sz < 1e-6f) sz = 1.0f;

    // For inverse of (R*S), we need (R*S)^-1 = S^-1 * R^T
    // Transpose the rotation part and divide by scale squared
    // (because R^T gives us R^-1 for orthonormal R, and we need to also invert scale)
    float inv_sx_sq = 1.0f / (sx * sx);
    float inv_sy_sq = 1.0f / (sy * sy);
    float inv_sz_sq = 1.0f / (sz * sz);

    result->matrix[0][0] = m.matrix[0][0] * inv_sx_sq;
    result->matrix[0][1] = m.matrix[1][0] * inv_sx_sq;
    result->matrix[0][2] = m.matrix[2][0] * inv_sx_sq;
    result->matrix[1][0] = m.matrix[0][1] * inv_sy_sq;
    result->matrix[1][1] = m.matrix[1][1] * inv_sy_sq;
    result->matrix[1][2] = m.matrix[2][1] * inv_sy_sq;
    result->matrix[2][0] = m.matrix[0][2] * inv_sz_sq;
    result->matrix[2][1] = m.matrix[1][2] * inv_sz_sq;
    result->matrix[2][2] = m.matrix[2][2] * inv_sz_sq;

    // Compute new translation: -R_inv * t
    float tx = m.matrix[0][3];
    float ty = m.matrix[1][3];
    float tz = m.matrix[2][3];

    result->matrix[0][3] = -(result->matrix[0][0] * tx + result->matrix[0][1] * ty + result->matrix[0][2] * tz);
    result->matrix[1][3] = -(result->matrix[1][0] * tx + result->matrix[1][1] * ty + result->matrix[1][2] * tz);
    result->matrix[2][3] = -(result->matrix[2][0] * tx + result->matrix[2][1] * ty + result->matrix[2][2] * tz);
}

void GlaParser::MultiplyMatrices(const GlaMatrix34& a, const GlaMatrix34& b,
                                  GlaMatrix34* result) const {
    // Treat as 4x4 matrices with implicit [0, 0, 0, 1] bottom row
    // result = a * b
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += a.matrix[i][k] * b.matrix[k][j];
            }
            // Handle implicit 4th row [0, 0, 0, 1]
            if (j == 3) {
                sum += a.matrix[i][3];  // * 1.0
            }
            result->matrix[i][j] = sum;
        }
    }
}

bool GlaParser::GetLocalBindPoseTransform(int bone_index,
                                           ozz::math::Float3* translation,
                                           ozz::math::Quaternion* rotation,
                                           ozz::math::Float3* scale) const {
    if (bone_index < 0 || static_cast<size_t>(bone_index) >= m_bones.size()) {
        return false;
    }

    const GlaBone& bone = m_bones[bone_index];

    // For root bone, extract transform directly from base_pose
    if (bone.parent < 0) {
        DecomposeMatrix(bone.base_pose, translation, rotation, scale);
        if (bone_index < 10) {
            ozz::log::Log() << "  Joint[" << bone_index << "]: " << bone.name
                            << " parent=" << bone.parent
                            << " trans=(" << translation->x << "," << translation->y << "," << translation->z << ")"
                            << " rot=(" << rotation->x << "," << rotation->y << "," << rotation->z << "," << rotation->w << ")"
                            << std::endl;
        }
        return true;
    }

    // For child bones, compute proper LOCAL transforms
    // local_rotation = parent_world_rotation^-1 * child_world_rotation
    // local_translation = parent_world_rotation^-1 * (child_world_pos - parent_world_pos)
    const GlaBone& parent_bone = m_bones[bone.parent];

    // Extract world transforms for parent and child
    ozz::math::Float3 parent_trans, parent_scale;
    ozz::math::Quaternion parent_rot;
    DecomposeMatrix(parent_bone.base_pose, &parent_trans, &parent_rot, &parent_scale);

    ozz::math::Float3 child_trans, child_scale;
    ozz::math::Quaternion child_rot;
    DecomposeMatrix(bone.base_pose, &child_trans, &child_rot, &child_scale);

    // Compute local rotation: parent_inv * child
    ozz::math::Quaternion parent_rot_inv = QuaternionConjugate(parent_rot);
    *rotation = QuaternionNormalize(QuaternionMultiply(parent_rot_inv, child_rot));

    // Compute world offset from parent to child
    ozz::math::Float3 world_offset(
        child_trans.x - parent_trans.x,
        child_trans.y - parent_trans.y,
        child_trans.z - parent_trans.z
    );

    // Rotate world offset into parent's local space
    *translation = RotateVectorByQuaternion(parent_rot_inv, world_offset);

    // Use uniform scale of 1 (scale is handled by coordinate conversion)
    *scale = ozz::math::Float3::one();

    // Debug first few
    if (bone_index < 10) {
        ozz::log::Log() << "  Joint[" << bone_index << "]: " << bone.name
                        << " parent=" << bone.parent
                        << " trans=(" << translation->x << "," << translation->y << "," << translation->z << ")"
                        << " rot=(" << rotation->x << "," << rotation->y << "," << rotation->z << "," << rotation->w << ")"
                        << std::endl;
    }

    return true;
}

void GlaParser::GetWorldPosition(int bone_index, float* x, float* y, float* z) const {
    if (bone_index < 0 || static_cast<size_t>(bone_index) >= m_bones.size()) {
        *x = *y = *z = 0.0f;
        return;
    }
    const GlaBone& bone = m_bones[bone_index];
    // Translation is in the 4th column (indices 3)
    *x = bone.base_pose.matrix[0][3];
    *y = bone.base_pose.matrix[1][3];
    *z = bone.base_pose.matrix[2][3];
}

// Compute animated world transform for a bone at a specific frame
// JKA formula:
//   bone_matrix = parent_bone_matrix * local_anim  (hierarchical)
//   world = bone_matrix * BasePoseMat
void GlaParser::ComputeAnimatedWorldTransform(int frame_index, int bone_index,
                                               ozz::math::Float3* world_pos,
                                               ozz::math::Quaternion* world_rot) const {
    // First compute bone_matrix hierarchically (like JKA's G2_TransformBone)
    float bone_matrix[3][4];
    ComputeBoneMatrix(frame_index, bone_index, bone_matrix);

    // Then compute world = bone_matrix * BasePoseMat
    const GlaBone& bone = m_bones[bone_index];
    float world_matrix[3][4];
    MultiplyMatrix34(bone_matrix, bone.base_pose.matrix, world_matrix);

    // Extract translation from world matrix
    *world_pos = ozz::math::Float3(
        world_matrix[0][3],
        world_matrix[1][3],
        world_matrix[2][3]);

    // Extract rotation from world matrix
    ozz::math::Float3 scale;
    DecomposeMatrix34(world_matrix, nullptr, world_rot, &scale);
}

// Compute bone_matrix recursively (hierarchical multiplication)
void GlaParser::ComputeBoneMatrix(int frame_index, int bone_index, float out[3][4]) const {
    const GlaBone& bone = m_bones[bone_index];

    // Get local animation transform
    ozz::math::Float3 anim_trans;
    ozz::math::Quaternion anim_rot;
    GetBoneTransform(frame_index, bone_index, &anim_trans, &anim_rot);

    // Build local animation matrix
    float local_anim[3][4];
    BuildMatrix34(anim_rot, anim_trans, local_anim);

    // Debug: print arm bones
    static bool debug_printed = false;
    if (!debug_printed && (bone_index == 25 || bone_index == 38)) {  // rhumerus or lhumerus
        printf("\n=== ComputeBoneMatrix debug for %s (bone %d) frame %d ===\n",
               bone.name, bone_index, frame_index);
        printf("  anim_trans: (%.2f, %.2f, %.2f)\n", anim_trans.x, anim_trans.y, anim_trans.z);
        printf("  anim_rot: (%.4f, %.4f, %.4f, %.4f)\n", anim_rot.x, anim_rot.y, anim_rot.z, anim_rot.w);
        printf("  local_anim matrix:\n");
        for (int i = 0; i < 3; i++) {
            printf("    [%.4f, %.4f, %.4f, %.4f]\n", local_anim[i][0], local_anim[i][1], local_anim[i][2], local_anim[i][3]);
        }
        if (bone_index == 38) debug_printed = true;  // Only print once
    }

    if (bone.parent < 0) {
        // Root bone: bone_matrix = identity * local_anim = local_anim
        // (In JKA this would be rootMatrix * local_anim, but we assume identity root)
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                out[i][j] = local_anim[i][j];
            }
        }
    } else {
        // Child bone: bone_matrix = parent_bone_matrix * local_anim
        float parent_matrix[3][4];
        ComputeBoneMatrix(frame_index, bone.parent, parent_matrix);
        MultiplyMatrix34(parent_matrix, local_anim, out);
    }
}

// Helper to decompose a 3x4 matrix (array version)
void GlaParser::DecomposeMatrix34(const float m[3][4],
                                   ozz::math::Float3* translation,
                                   ozz::math::Quaternion* rotation,
                                   ozz::math::Float3* scale) const {
    if (translation) {
        translation->x = m[0][3];
        translation->y = m[1][3];
        translation->z = m[2][3];
    }

    // Extract scale
    float sx = std::sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0]);
    float sy = std::sqrt(m[0][1] * m[0][1] + m[1][1] * m[1][1] + m[2][1] * m[2][1]);
    float sz = std::sqrt(m[0][2] * m[0][2] + m[1][2] * m[1][2] + m[2][2] * m[2][2]);

    if (scale) {
        *scale = ozz::math::Float3(sx, sy, sz);
    }

    if (rotation) {
        // Build normalized rotation matrix
        float r[3][3];
        if (sx > 1e-6f) {
            r[0][0] = m[0][0] / sx; r[1][0] = m[1][0] / sx; r[2][0] = m[2][0] / sx;
        } else {
            r[0][0] = 1; r[1][0] = 0; r[2][0] = 0;
        }
        if (sy > 1e-6f) {
            r[0][1] = m[0][1] / sy; r[1][1] = m[1][1] / sy; r[2][1] = m[2][1] / sy;
        } else {
            r[0][1] = 0; r[1][1] = 1; r[2][1] = 0;
        }
        if (sz > 1e-6f) {
            r[0][2] = m[0][2] / sz; r[1][2] = m[1][2] / sz; r[2][2] = m[2][2] / sz;
        } else {
            r[0][2] = 0; r[1][2] = 0; r[2][2] = 1;
        }

        // Convert to quaternion
        float trace = r[0][0] + r[1][1] + r[2][2];
        float qw, qx, qy, qz;
        if (trace > 0) {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            qw = 0.25f / s;
            qx = (r[2][1] - r[1][2]) * s;
            qy = (r[0][2] - r[2][0]) * s;
            qz = (r[1][0] - r[0][1]) * s;
        } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]);
            qw = (r[2][1] - r[1][2]) / s;
            qx = 0.25f * s;
            qy = (r[0][1] + r[1][0]) / s;
            qz = (r[0][2] + r[2][0]) / s;
        } else if (r[1][1] > r[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]);
            qw = (r[0][2] - r[2][0]) / s;
            qx = (r[0][1] + r[1][0]) / s;
            qy = 0.25f * s;
            qz = (r[1][2] + r[2][1]) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]);
            qw = (r[1][0] - r[0][1]) / s;
            qx = (r[0][2] + r[2][0]) / s;
            qy = (r[1][2] + r[2][1]) / s;
            qz = 0.25f * s;
        }
        *rotation = QuaternionNormalize(ozz::math::Quaternion(qx, qy, qz, qw));
    }
}

bool GlaParser::GetAnimTransformInParentSpace(int frame_index, int bone_index,
                                               ozz::math::Float3* translation,
                                               ozz::math::Quaternion* rotation) const {
    if (bone_index < 0 || static_cast<size_t>(bone_index) >= m_bones.size()) {
        return false;
    }

    const GlaBone& bone = m_bones[bone_index];

    // Compute animated world transform for this bone (in JKA space)
    ozz::math::Float3 child_world_pos;
    ozz::math::Quaternion child_world_rot;
    ComputeAnimatedWorldTransform(frame_index, bone_index, &child_world_pos, &child_world_rot);

    if (bone.parent < 0) {
        // Root bone: local = world (no parent to transform relative to)
        *translation = child_world_pos;
        *rotation = child_world_rot;
        return true;
    }

    // Compute parent's animated world transform (in JKA space)
    ozz::math::Float3 parent_world_pos;
    ozz::math::Quaternion parent_world_rot;
    ComputeAnimatedWorldTransform(frame_index, bone.parent, &parent_world_pos, &parent_world_rot);

    // Compute local transform: local = parent_world^-1 * child_world
    ozz::math::Quaternion parent_rot_inv = QuaternionConjugate(parent_world_rot);

    // Local rotation: parent_inv * child
    *rotation = QuaternionNormalize(QuaternionMultiply(parent_rot_inv, child_world_rot));

    // Local translation: parent_rot_inv * (child_pos - parent_pos)
    ozz::math::Float3 offset(
        child_world_pos.x - parent_world_pos.x,
        child_world_pos.y - parent_world_pos.y,
        child_world_pos.z - parent_world_pos.z);
    *translation = RotateVectorByQuaternion(parent_rot_inv, offset);

    return true;
}

void GlaParser::DebugTraceTransform(int frame_index, int bone_index) const {
    if (bone_index < 0 || static_cast<size_t>(bone_index) >= m_bones.size()) {
        return;
    }

    const GlaBone& bone = m_bones[bone_index];
    printf("\n=== DEBUG TRACE: Bone[%d] '%s' (parent=%d) frame=%d ===\n",
           bone_index, bone.name, bone.parent, frame_index);

    // Step 1: Raw animation delta from GLA
    ozz::math::Float3 anim_trans;
    ozz::math::Quaternion anim_rot;
    GetBoneTransform(frame_index, bone_index, &anim_trans, &anim_rot);
    printf("1. Raw anim delta (GLA local):\n");
    printf("   trans: (%.4f, %.4f, %.4f)\n", anim_trans.x, anim_trans.y, anim_trans.z);
    printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", anim_rot.x, anim_rot.y, anim_rot.z, anim_rot.w);

    // Step 2: Base pose (bind pose world transform)
    ozz::math::Float3 base_pos, base_scale;
    ozz::math::Quaternion base_rot;
    DecomposeMatrix(bone.base_pose, &base_pos, &base_rot, &base_scale);
    printf("2. Base pose (bind world):\n");
    printf("   pos:   (%.4f, %.4f, %.4f)\n", base_pos.x, base_pos.y, base_pos.z);
    printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", base_rot.x, base_rot.y, base_rot.z, base_rot.w);
    printf("   scale: (%.4f, %.4f, %.4f)\n", base_scale.x, base_scale.y, base_scale.z);

    // Step 3: Computed animated world = base_pose * anim_delta
    ozz::math::Float3 anim_world_pos;
    ozz::math::Quaternion anim_world_rot;
    ComputeAnimatedWorldTransform(frame_index, bone_index, &anim_world_pos, &anim_world_rot);
    printf("3. Animated world (base * anim):\n");
    printf("   pos:   (%.4f, %.4f, %.4f)\n", anim_world_pos.x, anim_world_pos.y, anim_world_pos.z);
    printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", anim_world_rot.x, anim_world_rot.y, anim_world_rot.z, anim_world_rot.w);

    if (bone.parent >= 0) {
        const GlaBone& parent_bone = m_bones[bone.parent];
        printf("4. Parent '%s' animated world:\n", parent_bone.name);

        ozz::math::Float3 parent_anim_pos;
        ozz::math::Quaternion parent_anim_rot;
        ComputeAnimatedWorldTransform(frame_index, bone.parent, &parent_anim_pos, &parent_anim_rot);
        printf("   pos:   (%.4f, %.4f, %.4f)\n", parent_anim_pos.x, parent_anim_pos.y, parent_anim_pos.z);
        printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", parent_anim_rot.x, parent_anim_rot.y, parent_anim_rot.z, parent_anim_rot.w);

        // Step 5: Computed local transform
        ozz::math::Float3 local_trans;
        ozz::math::Quaternion local_rot;
        GetAnimTransformInParentSpace(frame_index, bone_index, &local_trans, &local_rot);
        printf("5. Computed local (output to ozz, JKA coords):\n");
        printf("   trans: (%.4f, %.4f, %.4f)\n", local_trans.x, local_trans.y, local_trans.z);
        printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", local_rot.x, local_rot.y, local_rot.z, local_rot.w);

        // Step 6: Verify by recomposing: parent_world * local should = bone_world
        ozz::math::Quaternion recomp_rot = QuaternionNormalize(
            QuaternionMultiply(parent_anim_rot, local_rot));
        ozz::math::Float3 rotated_local = RotateVectorByQuaternion(parent_anim_rot, local_trans);
        ozz::math::Float3 recomp_pos(
            parent_anim_pos.x + rotated_local.x,
            parent_anim_pos.y + rotated_local.y,
            parent_anim_pos.z + rotated_local.z);
        printf("6. Verification (parent_world * local):\n");
        printf("   pos:   (%.4f, %.4f, %.4f)\n", recomp_pos.x, recomp_pos.y, recomp_pos.z);
        printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", recomp_rot.x, recomp_rot.y, recomp_rot.z, recomp_rot.w);

        // Check error
        float pos_err = std::sqrt(
            (recomp_pos.x - anim_world_pos.x) * (recomp_pos.x - anim_world_pos.x) +
            (recomp_pos.y - anim_world_pos.y) * (recomp_pos.y - anim_world_pos.y) +
            (recomp_pos.z - anim_world_pos.z) * (recomp_pos.z - anim_world_pos.z));
        printf("   Position error: %.6f\n", pos_err);
    } else {
        printf("4. (Root bone - no parent)\n");
        ozz::math::Float3 local_trans;
        ozz::math::Quaternion local_rot;
        GetAnimTransformInParentSpace(frame_index, bone_index, &local_trans, &local_rot);
        printf("5. Computed local (= world for root):\n");
        printf("   trans: (%.4f, %.4f, %.4f)\n", local_trans.x, local_trans.y, local_trans.z);
        printf("   rot:   (%.4f, %.4f, %.4f, %.4f)\n", local_rot.x, local_rot.y, local_rot.z, local_rot.w);
    }
    printf("=== END TRACE ===\n\n");
}

}  // namespace gla
