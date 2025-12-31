// ozz-retarget - Retarget animations between different skeleton formats
// Converts JKA animations to Mixamo skeleton format

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/soa_transform.h"

#include "bone_mapper.h"

namespace {

// Load a skeleton from ozz file
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

// Load an animation from ozz file
bool LoadAnimation(const std::string& path, ozz::animation::Animation* animation) {
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened()) {
        std::cerr << "Failed to open animation file: " << path << std::endl;
        return false;
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Animation>()) {
        std::cerr << "Invalid animation file: " << path << std::endl;
        return false;
    }

    archive >> *animation;
    return true;
}

// Save a built animation to ozz file
bool SaveAnimation(const std::string& path, const ozz::animation::Animation& animation) {
    ozz::io::File file(path.c_str(), "wb");
    if (!file.opened()) {
        std::cerr << "Failed to create output file: " << path << std::endl;
        return false;
    }

    ozz::io::OArchive archive(&file);
    archive << animation;
    return true;
}

// Get bone names from skeleton
std::vector<std::string> GetBoneNames(const ozz::animation::Skeleton& skeleton) {
    std::vector<std::string> names;
    names.reserve(skeleton.num_joints());
    for (int i = 0; i < skeleton.num_joints(); ++i) {
        names.push_back(skeleton.joint_names()[i]);
    }
    return names;
}

// Extract a single float from SoA format at given index
float GetSoaFloat(const ozz::math::SimdFloat4& soa, int index) {
    float values[4];
    ozz::math::StorePtrU(soa, values);
    return values[index];
}

// Extract translation from SoaTransform
ozz::math::Float3 ExtractTranslation(const ozz::math::SoaTransform& soa, int lane) {
    return ozz::math::Float3(
        GetSoaFloat(soa.translation.x, lane),
        GetSoaFloat(soa.translation.y, lane),
        GetSoaFloat(soa.translation.z, lane));
}

// Extract rotation from SoaTransform
ozz::math::Quaternion ExtractRotation(const ozz::math::SoaTransform& soa, int lane) {
    return ozz::math::Quaternion(
        GetSoaFloat(soa.rotation.x, lane),
        GetSoaFloat(soa.rotation.y, lane),
        GetSoaFloat(soa.rotation.z, lane),
        GetSoaFloat(soa.rotation.w, lane));
}

// Extract scale from SoaTransform
ozz::math::Float3 ExtractScale(const ozz::math::SoaTransform& soa, int lane) {
    return ozz::math::Float3(
        GetSoaFloat(soa.scale.x, lane),
        GetSoaFloat(soa.scale.y, lane),
        GetSoaFloat(soa.scale.z, lane));
}

// Quaternion inverse (conjugate for unit quaternions)
ozz::math::Quaternion QuatInverse(const ozz::math::Quaternion& q) {
    return ozz::math::Quaternion(-q.x, -q.y, -q.z, q.w);
}

// Quaternion multiplication: a * b
ozz::math::Quaternion QuatMul(const ozz::math::Quaternion& a, const ozz::math::Quaternion& b) {
    return ozz::math::Quaternion(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    );
}

// Normalize quaternion
ozz::math::Quaternion QuatNormalize(const ozz::math::Quaternion& q) {
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 0.0001f) {
        return ozz::math::Quaternion(q.x / len, q.y / len, q.z / len, q.w / len);
    }
    return ozz::math::Quaternion::identity();
}

// Get rest pose rotation for a joint
ozz::math::Quaternion GetRestPoseRotation(const ozz::animation::Skeleton& skeleton, int joint_idx) {
    int soa_idx = joint_idx / 4;
    int lane = joint_idx % 4;
    return ExtractRotation(skeleton.joint_rest_poses()[soa_idx], lane);
}

// Print quaternion for debugging
void PrintQuat(const char* label, const ozz::math::Quaternion& q) {
    // Convert to axis-angle for more intuitive reading
    float angle = 2.0f * std::acos(std::min(1.0f, std::max(-1.0f, q.w)));
    float s = std::sqrt(1.0f - q.w * q.w);
    float ax = 0, ay = 0, az = 1;
    if (s > 0.001f) {
        ax = q.x / s;
        ay = q.y / s;
        az = q.z / s;
    }
    float angle_deg = angle * 180.0f / 3.14159f;
    std::cout << "  " << label << ": quat(" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << ")"
              << " = " << angle_deg << "° around (" << ax << ", " << ay << ", " << az << ")" << std::endl;
}

// Retarget a rotation using relative motion retargeting
// Formula:
//   1. Extract motion delta from source: delta = anim_rotation * inverse(source_rest)
//   2. Apply delta to target rest pose: final = delta * target_rest
// This preserves the MOTION (rotation relative to rest) rather than the absolute rotation
ozz::math::Quaternion RetargetRotation(
    const ozz::math::Quaternion& source_rotation,
    const ozz::math::Quaternion& source_rest,
    const ozz::math::Quaternion& target_rest,
    const char* bone_name = nullptr,
    bool debug = false) {

    // Extract the motion delta: how much did we rotate from rest pose?
    ozz::math::Quaternion source_delta = QuatMul(source_rotation, QuatInverse(source_rest));

    // Apply the same motion delta to the target rest pose
    ozz::math::Quaternion result = QuatNormalize(QuatMul(source_delta, target_rest));

    if (debug && bone_name) {
        std::cout << "\n=== Retarget: " << bone_name << " ===" << std::endl;
        PrintQuat("source_rest", source_rest);
        PrintQuat("source_anim", source_rotation);
        PrintQuat("delta (anim * inv(rest))", source_delta);
        PrintQuat("target_rest", target_rest);
        PrintQuat("result (delta * target_rest)", result);
    }

    return result;
}

// Get transform for a specific joint at a specific time
retarget::BoneTransform SampleJoint(
    const ozz::animation::Animation& animation,
    const ozz::animation::Skeleton& skeleton,
    int joint_index,
    float time,
    ozz::animation::SamplingJob::Context* context,
    std::vector<ozz::math::SoaTransform>* locals) {

    // Sample the animation
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &animation;
    sampling_job.context = context;
    sampling_job.ratio = time / animation.duration();
    sampling_job.output = ozz::make_span(*locals);

    if (!sampling_job.Run()) {
        return retarget::BoneTransform::identity();
    }

    // Extract the joint transform from SoA format
    int soa_index = joint_index / 4;
    int lane = joint_index % 4;

    retarget::BoneTransform result;
    result.translation = ExtractTranslation((*locals)[soa_index], lane);
    result.rotation = ExtractRotation((*locals)[soa_index], lane);
    result.scale = ExtractScale((*locals)[soa_index], lane);

    return result;
}

// Bones to debug (when --debug is enabled)
bool ShouldDebugBone(const char* name) {
    // Debug full arm chain to understand hand orientation
    return strcmp(name, "mixamorig:Hips") == 0 ||
           strcmp(name, "mixamorig:Spine2") == 0 ||
           strcmp(name, "mixamorig:RightShoulder") == 0 ||
           strcmp(name, "mixamorig:RightArm") == 0 ||
           strcmp(name, "mixamorig:RightForeArm") == 0 ||
           strcmp(name, "mixamorig:RightHand") == 0 ||
           strcmp(name, "mixamorig:LeftShoulder") == 0 ||
           strcmp(name, "mixamorig:LeftArm") == 0 ||
           strcmp(name, "mixamorig:LeftForeArm") == 0 ||
           strcmp(name, "mixamorig:LeftHand") == 0;
}

// Compute world-space rotation by walking up the hierarchy
ozz::math::Quaternion ComputeWorldRotation(
    const std::vector<ozz::math::SoaTransform>& locals,
    const ozz::animation::Skeleton& skeleton,
    int joint_idx) {

    ozz::math::Quaternion world = ozz::math::Quaternion::identity();

    // Walk up the hierarchy
    int current = joint_idx;
    while (current >= 0) {
        int soa_idx = current / 4;
        int lane = current % 4;
        ozz::math::Quaternion local = ExtractRotation(locals[soa_idx], lane);
        world = QuatMul(local, world);  // parent * child
        current = skeleton.joint_parents()[current];
    }

    return world;
}

// Given a world rotation and parent world rotation, compute local rotation
ozz::math::Quaternion WorldToLocal(
    const ozz::math::Quaternion& world,
    const ozz::math::Quaternion& parent_world) {
    // local = inv(parent_world) * world
    return QuatMul(QuatInverse(parent_world), world);
}

void PrintUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --source-anim=FILE      Source animation file (.ozz)\n"
              << "  --source-skeleton=FILE  Source skeleton file (.ozz)\n"
              << "  --target-skeleton=FILE  Target skeleton file (.ozz)\n"
              << "  --config=FILE           Bone mapping config file (.json)\n"
              << "  --output=FILE           Output animation file (.ozz)\n"
              << "  --sample-rate=N         Samples per second (default: 30)\n"
              << "  --debug                 Print debug info for key bones\n"
              << "  --help                  Show this help message\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string source_anim_path;
    std::string source_skeleton_path;
    std::string target_skeleton_path;
    std::string config_path;
    std::string output_path;
    float sample_rate = 30.0f;
    bool debug_mode = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg.rfind("--source-anim=", 0) == 0) {
            source_anim_path = arg.substr(14);
        } else if (arg.rfind("--source-skeleton=", 0) == 0) {
            source_skeleton_path = arg.substr(18);
        } else if (arg.rfind("--target-skeleton=", 0) == 0) {
            target_skeleton_path = arg.substr(18);
        } else if (arg.rfind("--config=", 0) == 0) {
            config_path = arg.substr(9);
        } else if (arg.rfind("--output=", 0) == 0) {
            output_path = arg.substr(9);
        } else if (arg.rfind("--sample-rate=", 0) == 0) {
            sample_rate = std::stof(arg.substr(14));
        } else if (arg == "--debug") {
            debug_mode = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (source_anim_path.empty() || source_skeleton_path.empty() ||
        target_skeleton_path.empty() || config_path.empty() || output_path.empty()) {
        std::cerr << "Missing required arguments.\n";
        PrintUsage(argv[0]);
        return 1;
    }

    // Load source skeleton
    ozz::animation::Skeleton source_skeleton;
    if (!LoadSkeleton(source_skeleton_path, &source_skeleton)) {
        return 1;
    }
    std::cout << "Loaded source skeleton: " << source_skeleton.num_joints() << " joints" << std::endl;

    // Load target skeleton
    ozz::animation::Skeleton target_skeleton;
    if (!LoadSkeleton(target_skeleton_path, &target_skeleton)) {
        return 1;
    }
    std::cout << "Loaded target skeleton: " << target_skeleton.num_joints() << " joints" << std::endl;

    // Load source animation
    ozz::animation::Animation source_animation;
    if (!LoadAnimation(source_anim_path, &source_animation)) {
        return 1;
    }
    std::cout << "Loaded source animation: " << source_animation.duration() << "s, "
              << source_animation.num_tracks() << " tracks" << std::endl;

    // Load bone mapping config
    retarget::BoneMapper mapper;
    if (!mapper.LoadConfig(config_path)) {
        return 1;
    }

    // Build bone name maps
    auto source_names = GetBoneNames(source_skeleton);
    auto target_names = GetBoneNames(target_skeleton);
    mapper.BuildSourceBoneMap(source_names);
    mapper.BuildTargetBoneMap(target_names);

    // Create sampling context and buffer for source animation
    ozz::animation::SamplingJob::Context context;
    context.Resize(source_skeleton.num_joints());
    std::vector<ozz::math::SoaTransform> source_locals(source_skeleton.num_soa_joints());

    // Calculate number of keyframes
    float duration = source_animation.duration();
    int num_keyframes = static_cast<int>(duration * sample_rate) + 1;
    float time_step = duration / std::max(1, num_keyframes - 1);

    std::cout << "Retargeting with " << num_keyframes << " keyframes..." << std::endl;

    // Debug: Print skeleton rest poses
    if (debug_mode) {
        std::cout << "\n=== SOURCE SKELETON REST POSES ===" << std::endl;
        for (int i = 0; i < source_skeleton.num_joints(); ++i) {
            const char* name = source_skeleton.joint_names()[i];
            int parent = source_skeleton.joint_parents()[i];
            ozz::math::Quaternion rest = GetRestPoseRotation(source_skeleton, i);
            std::cout << "  [" << i << "] " << name << " (parent=" << parent << "): ";
            std::cout << "quat(" << rest.x << ", " << rest.y << ", " << rest.z << ", " << rest.w << ")" << std::endl;
        }

        std::cout << "\n=== TARGET SKELETON REST POSES ===" << std::endl;
        for (int i = 0; i < target_skeleton.num_joints(); ++i) {
            const char* name = target_skeleton.joint_names()[i];
            int parent = target_skeleton.joint_parents()[i];
            ozz::math::Quaternion rest = GetRestPoseRotation(target_skeleton, i);
            std::cout << "  [" << i << "] " << name << " (parent=" << parent << "): ";
            std::cout << "quat(" << rest.x << ", " << rest.y << ", " << rest.z << ", " << rest.w << ")" << std::endl;
        }
        std::cout << std::endl;
    }

    // Build raw animation for target skeleton
    ozz::animation::offline::RawAnimation raw_animation;
    raw_animation.duration = duration;
    raw_animation.name = "retargeted";
    raw_animation.tracks.resize(target_skeleton.num_joints());

    // Pre-compute mapping info for each target bone
    struct BoneMappingInfo {
        bool is_mapped = false;
        std::vector<int> source_indices;
        bool combine_rotations = false;
        ozz::math::Quaternion correction = ozz::math::Quaternion::identity();
        bool has_correction = false;
    };
    std::vector<BoneMappingInfo> mapping_infos(target_skeleton.num_joints());

    for (int target_idx = 0; target_idx < target_skeleton.num_joints(); ++target_idx) {
        const char* target_name = target_skeleton.joint_names()[target_idx];
        auto mapping_opt = mapper.GetMapping(target_name);

        if (mapping_opt && !mapping_opt->is_unmapped()) {
            mapping_infos[target_idx].is_mapped = true;
            mapping_infos[target_idx].combine_rotations = mapping_opt->is_combined();
            mapping_infos[target_idx].correction = mapping_opt->correction;
            mapping_infos[target_idx].has_correction = mapping_opt->has_correction;
            for (const auto& src_name : mapping_opt->source_bones) {
                int idx = mapper.GetSourceBoneIndex(src_name);
                if (idx >= 0) {
                    mapping_infos[target_idx].source_indices.push_back(idx);
                }
            }
        }
    }

    // Storage for world rotations during each frame
    std::vector<ozz::math::Quaternion> source_world_rotations(source_skeleton.num_joints());
    std::vector<ozz::math::Quaternion> target_world_rotations(target_skeleton.num_joints());

    // Process frame by frame (so we can compute world transforms)
    for (int frame = 0; frame < num_keyframes; ++frame) {
        float time = frame * time_step;

        // Sample source animation for this frame
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = &source_animation;
        sampling_job.context = &context;
        sampling_job.ratio = time / source_animation.duration();
        sampling_job.output = ozz::make_span(source_locals);
        if (!sampling_job.Run()) {
            std::cerr << "Sampling failed at frame " << frame << std::endl;
            return 1;
        }

        // Compute world rotations for all source bones
        for (int i = 0; i < source_skeleton.num_joints(); ++i) {
            int soa_idx = i / 4;
            int lane = i % 4;
            ozz::math::Quaternion local = ExtractRotation(source_locals[soa_idx], lane);

            int parent = source_skeleton.joint_parents()[i];
            if (parent < 0) {
                source_world_rotations[i] = local;
            } else {
                source_world_rotations[i] = QuatMul(source_world_rotations[parent], local);
            }
        }

        // Process each target bone
        for (int target_idx = 0; target_idx < target_skeleton.num_joints(); ++target_idx) {
            const char* target_name = target_skeleton.joint_names()[target_idx];
            auto& track = raw_animation.tracks[target_idx];
            const auto& info = mapping_infos[target_idx];

            // Get target parent world rotation
            int target_parent = target_skeleton.joint_parents()[target_idx];
            ozz::math::Quaternion target_parent_world =
                (target_parent < 0) ? ozz::math::Quaternion::identity()
                                    : target_world_rotations[target_parent];

            ozz::math::Quaternion local_rotation;
            ozz::math::Float3 translation;
            ozz::math::Float3 scale(1.0f, 1.0f, 1.0f);

            if (!info.is_mapped || info.source_indices.empty()) {
                // Unmapped bone - use rest pose
                int soa_idx = target_idx / 4;
                int lane = target_idx % 4;
                const auto& rest = target_skeleton.joint_rest_poses()[soa_idx];
                local_rotation = ExtractRotation(rest, lane);
                translation = ExtractTranslation(rest, lane);
                scale = ExtractScale(rest, lane);
            } else {
                // Get source world rotation (combine if needed)
                ozz::math::Quaternion source_world;
                if (info.combine_rotations && info.source_indices.size() > 1) {
                    // For combined bones, multiply the local rotations then compute world
                    int first_src = info.source_indices[0];
                    int first_soa = first_src / 4;
                    int first_lane = first_src % 4;
                    ozz::math::Quaternion combined_local = ExtractRotation(source_locals[first_soa], first_lane);

                    for (size_t j = 1; j < info.source_indices.size(); ++j) {
                        int src = info.source_indices[j];
                        int soa_idx = src / 4;
                        int lane = src % 4;
                        ozz::math::Quaternion rot = ExtractRotation(source_locals[soa_idx], lane);
                        combined_local = QuatMul(combined_local, rot);
                    }

                    // Get parent of first source bone for world transform
                    int first_parent = source_skeleton.joint_parents()[first_src];
                    ozz::math::Quaternion parent_world =
                        (first_parent < 0) ? ozz::math::Quaternion::identity()
                                           : source_world_rotations[first_parent];
                    source_world = QuatMul(parent_world, combined_local);

                    // Translation from first bone
                    translation = ExtractTranslation(source_locals[first_soa], first_lane);
                } else {
                    // Single source bone - use its world rotation
                    int src_idx = info.source_indices[0];
                    source_world = source_world_rotations[src_idx];

                    int soa_idx = src_idx / 4;
                    int lane = src_idx % 4;
                    translation = ExtractTranslation(source_locals[soa_idx], lane);
                }

                // Apply correction if specified
                // correction is applied to the world rotation to adjust for different bone axis conventions
                if (info.has_correction) {
                    source_world = QuatNormalize(QuatMul(source_world, info.correction));
                }

                // Convert source world to target local:
                // target_local = inv(target_parent_world) * source_world
                local_rotation = QuatNormalize(WorldToLocal(source_world, target_parent_world));

                // Debug output
                if (debug_mode && frame == 0 && ShouldDebugBone(target_name)) {
                    std::cout << "\n=== World-Space Retarget: " << target_name << " ===" << std::endl;
                    std::cout << "  Source bone(s): ";
                    for (size_t k = 0; k < info.source_indices.size(); ++k) {
                        if (k > 0) std::cout << " + ";
                        std::cout << source_skeleton.joint_names()[info.source_indices[k]];
                    }
                    std::cout << std::endl;
                    PrintQuat("source_world", source_world);
                    if (info.has_correction) {
                        PrintQuat("correction applied", info.correction);
                    }
                    PrintQuat("target_parent_world", target_parent_world);
                    PrintQuat("result_local", local_rotation);

                    // Also show what the target world will be
                    ozz::math::Quaternion target_world = QuatMul(target_parent_world, local_rotation);
                    PrintQuat("target_world (verify)", target_world);
                }
            }

            // Store target world rotation for child bones
            target_world_rotations[target_idx] = QuatMul(target_parent_world, local_rotation);

            // Add keyframes
            ozz::animation::offline::RawAnimation::TranslationKey t_key;
            t_key.time = time;
            t_key.value = translation;
            track.translations.push_back(t_key);

            ozz::animation::offline::RawAnimation::RotationKey r_key;
            r_key.time = time;
            r_key.value = local_rotation;
            track.rotations.push_back(r_key);

            ozz::animation::offline::RawAnimation::ScaleKey s_key;
            s_key.time = time;
            s_key.value = scale;
            track.scales.push_back(s_key);
        }
    }

    // Validate raw animation
    if (!raw_animation.Validate()) {
        std::cerr << "Raw animation validation failed" << std::endl;
        return 1;
    }

    // Build runtime animation
    ozz::animation::offline::AnimationBuilder builder;
    ozz::unique_ptr<ozz::animation::Animation> output_animation = builder(raw_animation);
    if (!output_animation) {
        std::cerr << "Failed to build animation" << std::endl;
        return 1;
    }

    // Save output animation
    if (!SaveAnimation(output_path, *output_animation)) {
        return 1;
    }

    std::cout << "Saved retargeted animation: " << output_path << std::endl;
    std::cout << "  Duration: " << output_animation->duration() << "s" << std::endl;
    std::cout << "  Tracks: " << output_animation->num_tracks() << std::endl;

    return 0;
}
