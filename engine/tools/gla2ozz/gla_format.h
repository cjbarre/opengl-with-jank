// GLA (Ghoul2 Animation) file format structures
// Reference: OpenJK codemp/rd-common/mdx_format.h

#ifndef GLA_FORMAT_H_
#define GLA_FORMAT_H_

#include <cstdint>

namespace gla {

// GLA file magic: "2LGA" (little-endian: 0x32 0x4C 0x47 0x41)
constexpr uint32_t GLA_IDENT = (('A' << 24) + ('G' << 16) + ('L' << 8) + '2');
constexpr int GLA_VERSION = 6;

// Maximum bone name length
constexpr int MAX_BONE_NAME = 64;

#pragma pack(push, 1)

// GLA file header (100 bytes)
struct GlaHeader {
    uint32_t ident;              // Magic: "2LGA"
    uint32_t version;            // Version: 6
    char name[MAX_BONE_NAME];    // Internal path/name
    float scale;                 // Build scale factor
    int32_t num_frames;          // Total animation frames
    int32_t ofs_frames;          // Offset to frame index data
    int32_t num_bones;           // Number of bones in skeleton
    int32_t ofs_comp_bone_pool;  // Offset to compressed bone pool
    int32_t ofs_skel;            // Offset to skeleton data
    int32_t ofs_end;             // File size / end marker
};

// 3x4 transformation matrix stored in GLA (row-major)
struct GlaMatrix34 {
    float matrix[3][4];
};

// Skeleton bone structure (variable size due to children array)
// This is the on-disk format; we read it field by field
struct GlaSkelBoneHeader {
    char name[MAX_BONE_NAME];    // Bone name
    uint32_t flags;              // Bone flags (e.g., ALWAYSXFORM = 0x01)
    int32_t parent;              // Parent bone index (-1 for root)
    GlaMatrix34 base_pose;       // Bind pose transformation
    GlaMatrix34 base_pose_inv;   // Inverse bind pose
    int32_t num_children;        // Number of child bones
    // Followed by: int32_t children[num_children]
};

// Compressed bone transform (14 bytes = 7 unsigned shorts)
// Quaternion + Translation packed into 14 bytes
struct CompressedBone {
    uint8_t data[14];
};

#pragma pack(pop)

// Parsed bone data (after reading from file)
struct GlaBone {
    char name[MAX_BONE_NAME];
    uint32_t flags;
    int32_t parent;
    GlaMatrix34 base_pose;
    GlaMatrix34 base_pose_inv;
    int32_t num_children;
};

// Bone transform decompression constants
// Quaternion components: value/16383 - 2 maps [0,65535] to [-2,2]
// (Actually uses different range - needs verification from OpenJK)
constexpr float QUAT_SCALE = 16383.0f;
constexpr float QUAT_OFFSET = 2.0f;

// Translation components: value/64 - 512 maps [0,65535] to [-512,512]
constexpr float TRANS_SCALE = 64.0f;
constexpr float TRANS_OFFSET = 512.0f;

}  // namespace gla

#endif  // GLA_FORMAT_H_
