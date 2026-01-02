// bone_mapper.h - Bone mapping configuration for skeleton retargeting
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"

namespace retarget {

// Represents a single bone mapping from target to source skeleton
struct BoneMapping {
    std::string target_bone;           // Mixamo bone name
    std::vector<std::string> source_bones;  // Source bone name(s) - can be multiple for combined bones
    bool combine_rotations = false;    // If true, multiply rotations of source bones
    ozz::math::Quaternion correction;  // Rotation correction to apply to world rotation
    bool has_correction = false;       // Whether a correction was specified

    // Single source bone mapping
    BoneMapping(const std::string& target, const std::string& source)
        : target_bone(target), source_bones{source}, combine_rotations(false),
          correction(ozz::math::Quaternion::identity()), has_correction(false) {}

    // Multiple source bones (combined)
    BoneMapping(const std::string& target, const std::vector<std::string>& sources)
        : target_bone(target), source_bones(sources), combine_rotations(sources.size() > 1),
          correction(ozz::math::Quaternion::identity()), has_correction(false) {}

    // Unmapped bone (identity transform)
    explicit BoneMapping(const std::string& target)
        : target_bone(target), source_bones{}, combine_rotations(false),
          correction(ozz::math::Quaternion::identity()), has_correction(false) {}

    bool is_unmapped() const { return source_bones.empty(); }
    bool is_combined() const { return source_bones.size() > 1; }
};

// Transform data for a single bone
struct BoneTransform {
    ozz::math::Float3 translation;
    ozz::math::Quaternion rotation;
    ozz::math::Float3 scale;

    BoneTransform()
        : translation(ozz::math::Float3::zero())
        , rotation(ozz::math::Quaternion::identity())
        , scale(ozz::math::Float3(1.0f, 1.0f, 1.0f)) {}

    BoneTransform(const ozz::math::Float3& t, const ozz::math::Quaternion& r)
        : translation(t), rotation(r), scale(ozz::math::Float3(1.0f, 1.0f, 1.0f)) {}

    static BoneTransform identity() { return BoneTransform(); }
};

// Main class for loading and applying bone mappings
class BoneMapper {
public:
    BoneMapper() = default;

    // Load mapping configuration from JSON file
    bool LoadConfig(const std::string& config_path);

    // Get mapping for a target bone
    // Returns nullopt if bone is not in the mapping config
    std::optional<BoneMapping> GetMapping(const std::string& target_bone) const;

    // Get all mappings
    const std::vector<BoneMapping>& GetMappings() const { return m_mappings; }

    // Build bone name to index maps from skeletons
    void BuildSourceBoneMap(const std::vector<std::string>& bone_names);
    void BuildTargetBoneMap(const std::vector<std::string>& bone_names);

    // Get source bone index by name (-1 if not found)
    int GetSourceBoneIndex(const std::string& name) const;

    // Get target bone index by name (-1 if not found)
    int GetTargetBoneIndex(const std::string& name) const;

    // Combine multiple rotations (quaternion multiplication)
    static ozz::math::Quaternion CombineRotations(
        const std::vector<ozz::math::Quaternion>& rotations);

    // Get number of mappings
    size_t GetMappingCount() const { return m_mappings.size(); }

private:
    std::vector<BoneMapping> m_mappings;
    std::map<std::string, int> m_source_bone_map;  // bone name -> index
    std::map<std::string, int> m_target_bone_map;  // bone name -> index
};

}  // namespace retarget
