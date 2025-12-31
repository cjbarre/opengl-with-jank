// GLA file parser
// Parses Jedi Academy Ghoul2 animation files

#ifndef GLA_PARSER_H_
#define GLA_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gla_format.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"

namespace gla {

class GlaParser {
 public:
    GlaParser();
    ~GlaParser();

    // Load a GLA file
    bool Load(const char* filename);

    // Accessors
    const GlaHeader& GetHeader() const { return m_header; }
    const std::vector<GlaBone>& GetBones() const { return m_bones; }
    int GetNumFrames() const { return m_header.num_frames; }
    int GetNumBones() const { return m_header.num_bones; }

    // Get decompressed bone transform for a specific frame and bone
    // Returns the transform in JKA coordinates (before conversion)
    bool GetBoneTransform(int frame_index, int bone_index,
                          ozz::math::Float3* translation,
                          ozz::math::Quaternion* rotation) const;

    // Get the bind pose transform for a bone (from base_pose matrix)
    // This is the WORLD-SPACE transform (bone-to-model)
    bool GetBindPoseTransform(int bone_index,
                              ozz::math::Float3* translation,
                              ozz::math::Quaternion* rotation,
                              ozz::math::Float3* scale) const;

    // Get the LOCAL bind pose transform (relative to parent)
    // This is what ozz-animation expects for skeleton joints
    bool GetLocalBindPoseTransform(int bone_index,
                                   ozz::math::Float3* translation,
                                   ozz::math::Quaternion* rotation,
                                   ozz::math::Float3* scale) const;

    // Get world-space position from base_pose matrix (for verification)
    void GetWorldPosition(int bone_index, float* x, float* y, float* z) const;

    // Convert animation transform from bone-local space to parent-local space
    // GLA: world = base_pose * anim_transform
    // ozz: world = parent_world * local_transform
    // We need: local_transform = parent_world_inv * base_pose * anim_transform
    bool GetAnimTransformInParentSpace(int frame_index, int bone_index,
                                        ozz::math::Float3* translation,
                                        ozz::math::Quaternion* rotation) const;

    // Compute animated world transform for a bone at a specific frame
    // Returns: world = base_pose * anim_delta
    void ComputeAnimatedWorldTransform(int frame_index, int bone_index,
                                        ozz::math::Float3* world_pos,
                                        ozz::math::Quaternion* world_rot) const;

    // Debug: Print detailed transform trace for a bone
    void DebugTraceTransform(int frame_index, int bone_index) const;

 private:
    bool ParseHeader();
    bool ParseSkeleton();
    bool LoadCompressedBonePool();
    bool LoadFrameIndices();

    // Read a 3-byte packed frame index
    uint32_t ReadFrameIndex(int frame, int bone) const;

    // Decompress a 14-byte compressed bone transform
    void DecompressBone(const CompressedBone& comp,
                        ozz::math::Float3* translation,
                        ozz::math::Quaternion* rotation) const;

    // Decompose a 3x4 matrix into translation, rotation, scale
    bool DecomposeMatrix(const GlaMatrix34& matrix,
                         ozz::math::Float3* translation,
                         ozz::math::Quaternion* rotation,
                         ozz::math::Float3* scale) const;

    // Invert a 3x4 affine matrix (assumes orthonormal rotation)
    void InvertMatrix(const GlaMatrix34& m, GlaMatrix34* result) const;

    // Multiply two 3x4 matrices (treating as 4x4 with implicit [0,0,0,1] row)
    void MultiplyMatrices(const GlaMatrix34& a, const GlaMatrix34& b,
                          GlaMatrix34* result) const;

    GlaHeader m_header;
    std::vector<GlaBone> m_bones;
    std::vector<uint8_t> m_file_data;

    // Pointers into m_file_data for quick access
    const uint8_t* m_frame_data;      // Frame index data
    const uint8_t* m_bone_pool;       // Compressed bone pool
    size_t m_frame_data_size;
    size_t m_bone_pool_size;
};

}  // namespace gla

#endif  // GLA_PARSER_H_
