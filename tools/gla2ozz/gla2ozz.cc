// gla2ozz - Convert Jedi Academy GLA animations to ozz-animation format
// Uses ozz's OzzImporter framework for integration with CLI and config system

#include <cstring>
#include <map>
#include <string>

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/raw_track.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"

#include "animation_cfg_parser.h"
#include "coordinate_convert.h"
#include "gla_format.h"
#include "gla_parser.h"

namespace {

class GlaImporter : public ozz::animation::offline::OzzImporter {
 public:
    GlaImporter() {}

 private:
    // Load the GLA file and associated animation.cfg
    bool Load(const char* _filename) override {
        ozz::log::Log() << "Loading GLA file: " << _filename << std::endl;

        if (!m_parser.Load(_filename)) {
            ozz::log::Err() << "Failed to parse GLA file." << std::endl;
            return false;
        }

        // Try to load animation.cfg from the same directory
        if (!m_cfg_parser.LoadFromGlaDirectory(_filename)) {
            ozz::log::Log() << "No animation.cfg found. "
                           << "Only skeleton export will be available." << std::endl;
        }

        m_gla_path = _filename;
        return true;
    }

    // Import skeleton from GLA
    bool Import(ozz::animation::offline::RawSkeleton* _skeleton,
                const NodeType& _types) override {
        (void)_types;  // GLA doesn't distinguish node types

        const auto& bones = m_parser.GetBones();
        if (bones.empty()) {
            ozz::log::Err() << "No bones found in GLA file." << std::endl;
            return false;
        }

        ozz::log::Log() << "Importing skeleton with " << bones.size()
                        << " bones..." << std::endl;

        // Find root bones (parent == -1)
        std::vector<int> root_indices;
        for (size_t i = 0; i < bones.size(); ++i) {
            if (bones[i].parent == -1) {
                root_indices.push_back(static_cast<int>(i));
            }
        }

        if (root_indices.empty()) {
            ozz::log::Err() << "No root bones found in skeleton." << std::endl;
            return false;
        }

        ozz::log::LogV() << "Found " << root_indices.size()
                         << " root bone(s)." << std::endl;

        // Build joint hierarchy
        _skeleton->roots.resize(root_indices.size());
        for (size_t i = 0; i < root_indices.size(); ++i) {
            BuildJointRecursive(bones, root_indices[i], &_skeleton->roots[i]);
        }

        if (!_skeleton->Validate()) {
            ozz::log::Err() << "Skeleton validation failed." << std::endl;
            return false;
        }

        ozz::log::Log() << "Skeleton import complete: "
                        << _skeleton->num_joints() << " joints." << std::endl;
        return true;
    }

    // Get list of animation names from animation.cfg
    AnimationNames GetAnimationNames() override {
        AnimationNames names;
        const auto& clips = m_cfg_parser.GetClips();
        for (const auto& clip : clips) {
            names.push_back(ozz::string(clip.name.c_str()));
        }
        return names;
    }

    // Import a specific animation clip
    bool Import(const char* _animation_name,
                const ozz::animation::Skeleton& _skeleton,
                float _sampling_rate,
                ozz::animation::offline::RawAnimation* _animation) override {

        const gla::AnimationClip* clip = m_cfg_parser.FindClip(_animation_name);
        if (!clip) {
            ozz::log::Err() << "Animation '" << _animation_name
                           << "' not found in animation.cfg." << std::endl;
            return false;
        }

        ozz::log::Log() << "Importing animation '" << _animation_name
                        << "' (frames " << clip->start_frame << "-"
                        << (clip->start_frame + clip->frame_count - 1)
                        << " @ " << clip->fps << " fps)..." << std::endl;

        // DEBUG: Print first frame animation data vs bind pose for first 5 bones
        ozz::log::Log() << "DEBUG: Comparing frame 0 animation vs bind pose:" << std::endl;
        for (int b = 0; b < std::min(5, m_parser.GetNumBones()); ++b) {
            ozz::math::Float3 raw_anim_trans, conv_anim_trans, bind_trans, bind_scale;
            ozz::math::Quaternion raw_anim_rot, conv_anim_rot, bind_rot;

            // Get raw animation frame 0 data
            m_parser.GetBoneTransform(clip->start_frame, b, &raw_anim_trans, &raw_anim_rot);

            // Get converted animation (in parent-local space)
            m_parser.GetAnimTransformInParentSpace(clip->start_frame, b, &conv_anim_trans, &conv_anim_rot);

            // Get bind pose local transform
            m_parser.GetLocalBindPoseTransform(b, &bind_trans, &bind_rot, &bind_scale);

            const char* bone_name = m_parser.GetBones()[b].name;
            int parent = m_parser.GetBones()[b].parent;

            ozz::log::Log() << "  Bone[" << b << "] '" << bone_name << "' (parent=" << parent << "):" << std::endl;
            ozz::log::Log() << "    Raw anim trans: (" << raw_anim_trans.x << ", " << raw_anim_trans.y << ", " << raw_anim_trans.z << ")" << std::endl;
            ozz::log::Log() << "    Conv anim trans: (" << conv_anim_trans.x << ", " << conv_anim_trans.y << ", " << conv_anim_trans.z << ")" << std::endl;
            ozz::log::Log() << "    Bind trans: (" << bind_trans.x << ", " << bind_trans.y << ", " << bind_trans.z << ")" << std::endl;
            ozz::log::Log() << "    Conv anim rot: (" << conv_anim_rot.x << ", " << conv_anim_rot.y << ", " << conv_anim_rot.z << ", " << conv_anim_rot.w << ")" << std::endl;
            ozz::log::Log() << "    Bind rot: (" << bind_rot.x << ", " << bind_rot.y << ", " << bind_rot.z << ", " << bind_rot.w << ")" << std::endl;
        }

        // Validate frame range
        if (clip->start_frame + clip->frame_count > m_parser.GetNumFrames()) {
            ozz::log::Err() << "Animation frame range exceeds GLA file ("
                           << m_parser.GetNumFrames() << " frames)." << std::endl;
            return false;
        }

        // Use clip's FPS if valid, otherwise use sampling rate
        float fps = clip->fps > 0.0f ? clip->fps : _sampling_rate;
        if (fps <= 0.0f) {
            fps = 20.0f;  // Default JKA animation speed
        }
        float frame_duration = 1.0f / fps;

        _animation->name = _animation_name;
        // ozz requires duration > 0, so single-frame clips get a minimal duration
        _animation->duration = (clip->frame_count > 1) ?
            static_cast<float>(clip->frame_count - 1) * frame_duration :
            frame_duration;  // Single frame gets 1 frame duration

        // Build bone name to skeleton joint index map
        const int num_joints = _skeleton.num_joints();
        std::map<std::string, int> joint_map;
        for (int i = 0; i < num_joints; ++i) {
            joint_map[_skeleton.joint_names()[i]] = i;
        }

        // Initialize tracks for all joints
        _animation->tracks.resize(num_joints);

        // Track which joints we actually animated
        std::vector<bool> animated(num_joints, false);

        // DEBUG: Trace problematic bones for first frame only
        if (clip->start_frame > 0) {  // Only for actual animations, not idle
            int debug_frame = clip->start_frame;
            printf("\n\n*** DEBUG TRACES for animation '%s' frame %d ***\n", clip->name.c_str(), debug_frame);

            // Compare frame 0 animation data vs computed rest local from base_pose
            printf("\n=== FRAME %d vs BASE_POSE COMPARISON ===\n", clip->start_frame);
            int test_bones[] = {11, 12, 13, 24, 25, 27, 29};  // spine + arm chain
            for (int b : test_bones) {
                const auto& bone = m_parser.GetBones()[b];

                // Get animation data at clip start frame (not frame 0!)
                ozz::math::Float3 t0;
                ozz::math::Quaternion r0;
                m_parser.GetBoneTransform(clip->start_frame, b, &t0, &r0);

                // Get base_pose world position
                ozz::math::Float3 base_pos, base_scale;
                ozz::math::Quaternion base_rot;
                m_parser.DecomposeMatrix(bone.base_pose, &base_pos, &base_rot, &base_scale);

                printf("  %s:\n", bone.name);
                printf("    Frame 0 anim trans: (%.2f, %.2f, %.2f)\n", t0.x, t0.y, t0.z);
                printf("    Base_pose world pos: (%.2f, %.2f, %.2f)\n", base_pos.x, base_pos.y, base_pos.z);
                printf("    Difference: (%.2f, %.2f, %.2f)\n",
                       t0.x - base_pos.x, t0.y - base_pos.y, t0.z - base_pos.z);
                printf("    Ratio: (%.2f, %.2f, %.2f)\n",
                       base_pos.x != 0 ? t0.x / base_pos.x : 0,
                       base_pos.y != 0 ? t0.y / base_pos.y : 0,
                       base_pos.z != 0 ? t0.z / base_pos.z : 0);
            }
            printf("=== END COMPARISON ===\n\n");

            // Also print rest local comparison
            printf("=== REST LOCAL COMPARISON at frame %d (spine vs arms) ===\n", clip->start_frame);
            int test_bones2[] = {11, 12, 13, 25, 38};  // lower_lumbar, upper_lumbar, thoracic, rhumerus, lhumerus
            for (int b : test_bones2) {
                const auto& bone = m_parser.GetBones()[b];

                // Get animation data at clip start frame
                ozz::math::Float3 t0;
                ozz::math::Quaternion r0;
                m_parser.GetBoneTransform(clip->start_frame, b, &t0, &r0);

                // Compute rest local from base_pose matrices
                ozz::math::Float3 bone_world_pos, bone_scale;
                ozz::math::Quaternion bone_world_rot;
                m_parser.DecomposeMatrix(bone.base_pose, &bone_world_pos, &bone_world_rot, &bone_scale);

                ozz::math::Float3 rest_local_trans;
                if (bone.parent >= 0) {
                    const auto& parent = m_parser.GetBones()[bone.parent];
                    ozz::math::Float3 parent_pos, parent_scale;
                    ozz::math::Quaternion parent_rot;
                    m_parser.DecomposeMatrix(parent.base_pose, &parent_pos, &parent_rot, &parent_scale);

                    // Local offset in world space
                    ozz::math::Float3 world_offset(
                        bone_world_pos.x - parent_pos.x,
                        bone_world_pos.y - parent_pos.y,
                        bone_world_pos.z - parent_pos.z);

                    // Rotate by parent inverse to get local (inline quaternion rotation)
                    ozz::math::Quaternion q(-parent_rot.x, -parent_rot.y, -parent_rot.z, parent_rot.w);
                    ozz::math::Quaternion v_q(world_offset.x, world_offset.y, world_offset.z, 0.0f);
                    // temp = q * v_q
                    ozz::math::Quaternion temp(
                        q.w * v_q.x + q.x * v_q.w + q.y * v_q.z - q.z * v_q.y,
                        q.w * v_q.y - q.x * v_q.z + q.y * v_q.w + q.z * v_q.x,
                        q.w * v_q.z + q.x * v_q.y - q.y * v_q.x + q.z * v_q.w,
                        q.w * v_q.w - q.x * v_q.x - q.y * v_q.y - q.z * v_q.z);
                    // result = temp * conj(q)
                    ozz::math::Quaternion qc(q.x, q.y, q.z, q.w);  // conjugate of inverse = original
                    rest_local_trans.x = temp.w * qc.x + temp.x * qc.w + temp.y * qc.z - temp.z * qc.y;
                    rest_local_trans.y = temp.w * qc.y - temp.x * qc.z + temp.y * qc.w + temp.z * qc.x;
                    rest_local_trans.z = temp.w * qc.z + temp.x * qc.y - temp.y * qc.x + temp.z * qc.w;
                } else {
                    rest_local_trans = bone_world_pos;
                }

                printf("  %s (parent=%s):\n", bone.name, bone.parent >= 0 ? m_parser.GetBones()[bone.parent].name : "none");
                printf("    Frame 0 anim:   (%.2f, %.2f, %.2f)\n", t0.x, t0.y, t0.z);
                printf("    Rest local:     (%.2f, %.2f, %.2f)\n", rest_local_trans.x, rest_local_trans.y, rest_local_trans.z);
            }
            printf("=== END REST LOCAL COMPARISON ===\n\n");

            // Print full bone list first
            printf("\n=== FULL SKELETON BONE LIST ===\n");
            for (int b = 0; b < m_parser.GetNumBones(); ++b) {
                const auto& bone = m_parser.GetBones()[b];
                printf("  Bone[%d]: '%s' parent=%d\n", b, bone.name, bone.parent);
            }
            printf("=== END BONE LIST ===\n\n");

            // Trace spine chain
            m_parser.DebugTraceTransform(debug_frame, 1);   // pelvis
            m_parser.DebugTraceTransform(debug_frame, 11);  // lower_lumbar
            m_parser.DebugTraceTransform(debug_frame, 12);  // upper_lumbar
            m_parser.DebugTraceTransform(debug_frame, 13);  // thoracic

            // Trace RIGHT arm chain: thoracic -> rhumerus -> rradius -> rhand
            m_parser.DebugTraceTransform(debug_frame, 24);  // rclavical
            m_parser.DebugTraceTransform(debug_frame, 25);  // rhumerus
            m_parser.DebugTraceTransform(debug_frame, 27);  // rradius
            m_parser.DebugTraceTransform(debug_frame, 29);  // rhand

            // Trace LEFT arm chain: thoracic -> lhumerus -> lradius -> lhand
            m_parser.DebugTraceTransform(debug_frame, 37);  // lclavical
            m_parser.DebugTraceTransform(debug_frame, 38);  // lhumerus
            m_parser.DebugTraceTransform(debug_frame, 40);  // lradius
            m_parser.DebugTraceTransform(debug_frame, 42);  // lhand

            printf("*** END DEBUG TRACES ***\n\n");
        }

        // Build reverse map: ozz joint index -> GLA bone index
        std::map<int, int> ozz_to_gla;
        for (int b = 0; b < m_parser.GetNumBones(); ++b) {
            const char* bone_name = m_parser.GetBones()[b].name;
            auto it = joint_map.find(bone_name);
            if (it != joint_map.end()) {
                ozz_to_gla[it->second] = b;
            }
        }

        // Allocate storage for world transforms indexed by ozz joint
        std::vector<ozz::math::Float3> world_pos_ozz(num_joints);
        std::vector<ozz::math::Quaternion> world_rot_ozz(num_joints);

        // For each frame in the clip
        for (int f = 0; f < clip->frame_count; ++f) {
            int gla_frame = clip->start_frame + f;
            float time = static_cast<float>(f) * frame_duration;

            // Phase 1: Compute and convert world transforms for all ozz joints
            for (int j = 0; j < num_joints; ++j) {
                auto gla_it = ozz_to_gla.find(j);
                if (gla_it == ozz_to_gla.end()) {
                    // No GLA bone for this joint - use identity
                    world_pos_ozz[j] = ozz::math::Float3::zero();
                    world_rot_ozz[j] = ozz::math::Quaternion::identity();
                    continue;
                }
                int gla_bone = gla_it->second;

                ozz::math::Float3 world_pos_jka;
                ozz::math::Quaternion world_rot_jka;
                m_parser.ComputeAnimatedWorldTransform(gla_frame, gla_bone, &world_pos_jka, &world_rot_jka);

                // Convert world transform from JKA (Z-up) to ozz (Y-up)
                world_pos_ozz[j] = coord_convert::ConvertPosition(world_pos_jka);
                world_rot_ozz[j] = coord_convert::NormalizeQuaternion(
                    coord_convert::ConvertQuaternion(world_rot_jka));

                // Debug: print world transform for first frame, key joints
                const char* joint_name = _skeleton.joint_names()[j];
                bool is_key_joint = (j < 3 || strstr(joint_name, "humerus") != nullptr);
                if (f == 0 && is_key_joint) {
                    printf("  [%s] Joint[%d] '%s' GLA bone %d, frame %d:\n",
                           clip->name.c_str(), j, joint_name, gla_bone, gla_frame);
                    printf("    JKA world: (%.2f, %.2f, %.2f)\n",
                           world_pos_jka.x, world_pos_jka.y, world_pos_jka.z);
                    printf("    OZZ world: (%.4f, %.4f, %.4f)\n",
                           world_pos_ozz[j].x, world_pos_ozz[j].y, world_pos_ozz[j].z);
                }
            }

            // Phase 2: Compute local transforms using ozz's parent hierarchy
            for (int j = 0; j < num_joints; ++j) {
                auto& track = _animation->tracks[j];
                animated[j] = true;

                int ozz_parent = _skeleton.joint_parents()[j];

                ozz::math::Float3 trans;
                ozz::math::Quaternion rot;

                if (ozz_parent < 0) {
                    // Root joint: local = world
                    trans = world_pos_ozz[j];
                    rot = world_rot_ozz[j];
                } else {
                    // Non-root: compute local from ozz parent and child world transforms
                    ozz::math::Quaternion parent_rot_inv(
                        -world_rot_ozz[ozz_parent].x,
                        -world_rot_ozz[ozz_parent].y,
                        -world_rot_ozz[ozz_parent].z,
                        world_rot_ozz[ozz_parent].w);

                    // Local rotation: parent_inv * child
                    rot = coord_convert::NormalizeQuaternion(ozz::math::Quaternion(
                        parent_rot_inv.w * world_rot_ozz[j].x + parent_rot_inv.x * world_rot_ozz[j].w +
                            parent_rot_inv.y * world_rot_ozz[j].z - parent_rot_inv.z * world_rot_ozz[j].y,
                        parent_rot_inv.w * world_rot_ozz[j].y - parent_rot_inv.x * world_rot_ozz[j].z +
                            parent_rot_inv.y * world_rot_ozz[j].w + parent_rot_inv.z * world_rot_ozz[j].x,
                        parent_rot_inv.w * world_rot_ozz[j].z + parent_rot_inv.x * world_rot_ozz[j].y -
                            parent_rot_inv.y * world_rot_ozz[j].x + parent_rot_inv.z * world_rot_ozz[j].w,
                        parent_rot_inv.w * world_rot_ozz[j].w - parent_rot_inv.x * world_rot_ozz[j].x -
                            parent_rot_inv.y * world_rot_ozz[j].y - parent_rot_inv.z * world_rot_ozz[j].z));

                    // World offset
                    ozz::math::Float3 offset(
                        world_pos_ozz[j].x - world_pos_ozz[ozz_parent].x,
                        world_pos_ozz[j].y - world_pos_ozz[ozz_parent].y,
                        world_pos_ozz[j].z - world_pos_ozz[ozz_parent].z);

                    // Rotate offset by parent_inv: q * v * q^-1
                    ozz::math::Quaternion offset_q(offset.x, offset.y, offset.z, 0.0f);
                    ozz::math::Quaternion temp(
                        parent_rot_inv.w * offset_q.x + parent_rot_inv.x * offset_q.w +
                            parent_rot_inv.y * offset_q.z - parent_rot_inv.z * offset_q.y,
                        parent_rot_inv.w * offset_q.y - parent_rot_inv.x * offset_q.z +
                            parent_rot_inv.y * offset_q.w + parent_rot_inv.z * offset_q.x,
                        parent_rot_inv.w * offset_q.z + parent_rot_inv.x * offset_q.y -
                            parent_rot_inv.y * offset_q.x + parent_rot_inv.z * offset_q.w,
                        parent_rot_inv.w * offset_q.w - parent_rot_inv.x * offset_q.x -
                            parent_rot_inv.y * offset_q.y - parent_rot_inv.z * offset_q.z);
                    ozz::math::Quaternion parent_conj(
                        -parent_rot_inv.x, -parent_rot_inv.y, -parent_rot_inv.z, parent_rot_inv.w);
                    ozz::math::Quaternion result(
                        temp.w * parent_conj.x + temp.x * parent_conj.w +
                            temp.y * parent_conj.z - temp.z * parent_conj.y,
                        temp.w * parent_conj.y - temp.x * parent_conj.z +
                            temp.y * parent_conj.w + temp.z * parent_conj.x,
                        temp.w * parent_conj.z + temp.x * parent_conj.y -
                            temp.y * parent_conj.x + temp.z * parent_conj.w,
                        temp.w * parent_conj.w - temp.x * parent_conj.x -
                            temp.y * parent_conj.y - temp.z * parent_conj.z);
                    trans = ozz::math::Float3(result.x, result.y, result.z);
                }

                // Debug: print first frame for arm joints (rhumerus, lhumerus)
                const char* joint_name = _skeleton.joint_names()[j];
                bool is_arm_joint = (strstr(joint_name, "humerus") != nullptr);
                if (f == 0 && (j < 5 || is_arm_joint)) {
                    ozz::log::Log() << "  ANIMATION Joint[" << j << "] '" << _skeleton.joint_names()[j] << "':" << std::endl;
                    ozz::log::Log() << "    OZZ world pos: (" << world_pos_ozz[j].x << "," << world_pos_ozz[j].y << "," << world_pos_ozz[j].z << ")" << std::endl;
                    ozz::log::Log() << "    OZZ world rot: (" << world_rot_ozz[j].x << "," << world_rot_ozz[j].y << "," << world_rot_ozz[j].z << "," << world_rot_ozz[j].w << ")" << std::endl;
                    ozz::log::Log() << "    Computed local trans: (" << trans.x << "," << trans.y << "," << trans.z << ")" << std::endl;
                    ozz::log::Log() << "    Computed local rot:   (" << rot.x << "," << rot.y << "," << rot.z << "," << rot.w << ")" << std::endl;
                    if (ozz_parent >= 0) {
                        ozz::log::Log() << "    Parent[" << ozz_parent << "] world rot: ("
                            << world_rot_ozz[ozz_parent].x << "," << world_rot_ozz[ozz_parent].y << ","
                            << world_rot_ozz[ozz_parent].z << "," << world_rot_ozz[ozz_parent].w << ")" << std::endl;
                    }
                }

                // Add keyframes
                ozz::animation::offline::RawAnimation::TranslationKey trans_key;
                trans_key.time = time;
                trans_key.value = trans;
                track.translations.push_back(trans_key);

                ozz::animation::offline::RawAnimation::RotationKey rot_key;
                rot_key.time = time;
                rot_key.value = rot;
                track.rotations.push_back(rot_key);

                ozz::animation::offline::RawAnimation::ScaleKey scale_key;
                scale_key.time = time;
                scale_key.value = ozz::math::Float3::one();
                track.scales.push_back(scale_key);
            }
        }

        // Fill in missing tracks with rest pose and ensure proper keyframe count
        // ozz requires at least 2 keyframes with strictly ascending times for duration > 0
        for (int i = 0; i < num_joints; ++i) {
            auto& track = _animation->tracks[i];

            // Translations
            if (!animated[i] || track.translations.empty()) {
                ozz::animation::offline::RawAnimation::TranslationKey key;
                key.time = 0.0f;
                key.value = ozz::math::Float3::zero();
                track.translations.clear();
                track.translations.push_back(key);
            }
            // Add end keyframe for single-keyframe tracks
            if (track.translations.size() == 1 && _animation->duration > 0.0f) {
                ozz::animation::offline::RawAnimation::TranslationKey key;
                key.time = _animation->duration;
                key.value = track.translations[0].value;
                track.translations.push_back(key);
            }

            // Rotations
            if (!animated[i] || track.rotations.empty()) {
                ozz::animation::offline::RawAnimation::RotationKey key;
                key.time = 0.0f;
                key.value = ozz::math::Quaternion::identity();
                track.rotations.clear();
                track.rotations.push_back(key);
            }
            if (track.rotations.size() == 1 && _animation->duration > 0.0f) {
                ozz::animation::offline::RawAnimation::RotationKey key;
                key.time = _animation->duration;
                key.value = track.rotations[0].value;
                track.rotations.push_back(key);
            }

            // Scales
            if (!animated[i] || track.scales.empty()) {
                ozz::animation::offline::RawAnimation::ScaleKey key;
                key.time = 0.0f;
                key.value = ozz::math::Float3::one();
                track.scales.clear();
                track.scales.push_back(key);
            }
            if (track.scales.size() == 1 && _animation->duration > 0.0f) {
                ozz::animation::offline::RawAnimation::ScaleKey key;
                key.time = _animation->duration;
                key.value = track.scales[0].value;
                track.scales.push_back(key);
            }
        }

        if (!_animation->Validate()) {
            ozz::log::Err() << "Animation validation failed." << std::endl;
            return false;
        }

        ozz::log::Log() << "Animation import complete: "
                        << _animation->duration << " seconds, "
                        << clip->frame_count << " frames." << std::endl;
        return true;
    }

    // Track/property import - not supported for GLA
    NodeProperties GetNodeProperties(const char* _node_name) override {
        (void)_node_name;
        return NodeProperties();
    }

    bool Import(const char* _animation_name, const char* _node_name,
                const char* _track_name, NodeProperty::Type _track_type,
                float _sampling_rate,
                ozz::animation::offline::RawFloatTrack* _track) override {
        (void)_animation_name; (void)_node_name; (void)_track_name;
        (void)_track_type; (void)_sampling_rate; (void)_track;
        return false;
    }

    bool Import(const char* _animation_name, const char* _node_name,
                const char* _track_name, NodeProperty::Type _track_type,
                float _sampling_rate,
                ozz::animation::offline::RawFloat2Track* _track) override {
        (void)_animation_name; (void)_node_name; (void)_track_name;
        (void)_track_type; (void)_sampling_rate; (void)_track;
        return false;
    }

    bool Import(const char* _animation_name, const char* _node_name,
                const char* _track_name, NodeProperty::Type _track_type,
                float _sampling_rate,
                ozz::animation::offline::RawFloat3Track* _track) override {
        (void)_animation_name; (void)_node_name; (void)_track_name;
        (void)_track_type; (void)_sampling_rate; (void)_track;
        return false;
    }

    bool Import(const char* _animation_name, const char* _node_name,
                const char* _track_name, NodeProperty::Type _track_type,
                float _sampling_rate,
                ozz::animation::offline::RawFloat4Track* _track) override {
        (void)_animation_name; (void)_node_name; (void)_track_name;
        (void)_track_type; (void)_sampling_rate; (void)_track;
        return false;
    }

 private:
    // Recursively build joint hierarchy
    void BuildJointRecursive(
        const std::vector<gla::GlaBone>& bones,
        int bone_index,
        ozz::animation::offline::RawSkeleton::Joint* joint) {

        const gla::GlaBone& bone = bones[bone_index];
        joint->name = bone.name;

        // Get LOCAL bind pose transform (relative to parent)
        ozz::math::Float3 trans_jka, scale_jka;
        ozz::math::Quaternion rot_jka;
        m_parser.GetLocalBindPoseTransform(bone_index, &trans_jka, &rot_jka, &scale_jka);

        // Convert from JKA coordinates to ozz coordinates
        joint->transform.translation = coord_convert::ConvertPosition(trans_jka);
        joint->transform.rotation = coord_convert::NormalizeQuaternion(
            coord_convert::ConvertQuaternion(rot_jka));
        joint->transform.scale = ozz::math::Float3::one();  // Ignore scale for now

        // Debug: print first few bones with full quaternion info
        if (bone_index < 5) {
            ozz::log::Log() << "  SKELETON Joint[" << bone_index << "]: " << joint->name
                            << " parent=" << bone.parent << std::endl;
            ozz::log::Log() << "    JKA local trans: (" << trans_jka.x << "," << trans_jka.y << "," << trans_jka.z << ")" << std::endl;
            ozz::log::Log() << "    JKA local rot:   (" << rot_jka.x << "," << rot_jka.y << "," << rot_jka.z << "," << rot_jka.w << ")" << std::endl;
            ozz::log::Log() << "    OZZ local trans: (" << joint->transform.translation.x << ","
                            << joint->transform.translation.y << ","
                            << joint->transform.translation.z << ")" << std::endl;
            ozz::log::Log() << "    OZZ local rot:   (" << joint->transform.rotation.x << ","
                            << joint->transform.rotation.y << ","
                            << joint->transform.rotation.z << ","
                            << joint->transform.rotation.w << ")" << std::endl;
        }

        // Find and add children
        for (size_t i = 0; i < bones.size(); ++i) {
            if (bones[i].parent == bone_index) {
                joint->children.emplace_back();
                BuildJointRecursive(bones, static_cast<int>(i),
                                    &joint->children.back());
            }
        }
    }

    gla::GlaParser m_parser;
    gla::AnimationCfgParser m_cfg_parser;
    std::string m_gla_path;
};

}  // namespace

int main(int _argc, const char** _argv) {
    GlaImporter importer;
    return importer(_argc, _argv);
}
