// Unit tests for ozz-retarget
// Tests bone mapping, transform combining, and animation retargeting

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "bone_mapper.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"

// Test helpers
bool float_eq(float a, float b, float epsilon = 0.001f) {
    return std::abs(a - b) < epsilon;
}

bool quat_eq(const ozz::math::Quaternion& a, const ozz::math::Quaternion& b, float epsilon = 0.001f) {
    // Quaternions q and -q represent the same rotation
    bool same = float_eq(a.x, b.x, epsilon) && float_eq(a.y, b.y, epsilon) &&
                float_eq(a.z, b.z, epsilon) && float_eq(a.w, b.w, epsilon);
    bool neg = float_eq(a.x, -b.x, epsilon) && float_eq(a.y, -b.y, epsilon) &&
               float_eq(a.z, -b.z, epsilon) && float_eq(a.w, -b.w, epsilon);
    return same || neg;
}

// ============================================================================
// Bone Mapper Tests
// ============================================================================

void test_load_config_single_bone() {
    printf("Test: Load config with single bone mapping... ");

    // Create a temporary config file
    const char* config_content = R"({
        "mappings": [
            {"target": "mixamorig:Hips", "source": "pelvis"},
            {"target": "mixamorig:Spine", "source": "lower_lumbar"}
        ]
    })";

    std::string config_path = "/tmp/test_single_bone.json";
    {
        std::ofstream file(config_path);
        file << config_content;
    }

    retarget::BoneMapper mapper;
    assert(mapper.LoadConfig(config_path));
    assert(mapper.GetMappingCount() == 2);

    auto mapping = mapper.GetMapping("mixamorig:Hips");
    assert(mapping.has_value());
    assert(mapping->target_bone == "mixamorig:Hips");
    assert(mapping->source_bones.size() == 1);
    assert(mapping->source_bones[0] == "pelvis");
    assert(!mapping->is_unmapped());
    assert(!mapping->is_combined());

    printf("PASSED\n");
}

void test_load_config_combined_bones() {
    printf("Test: Load config with combined bone mapping... ");

    const char* config_content = R"({
        "mappings": [
            {"target": "mixamorig:LeftUpLeg", "source": ["lfemurYZ", "lfemurX"]}
        ]
    })";

    std::string config_path = "/tmp/test_combined.json";
    {
        std::ofstream file(config_path);
        file << config_content;
    }

    retarget::BoneMapper mapper;
    assert(mapper.LoadConfig(config_path));

    auto mapping = mapper.GetMapping("mixamorig:LeftUpLeg");
    assert(mapping.has_value());
    assert(mapping->source_bones.size() == 2);
    assert(mapping->source_bones[0] == "lfemurYZ");
    assert(mapping->source_bones[1] == "lfemurX");
    assert(mapping->is_combined());

    printf("PASSED\n");
}

void test_load_config_unmapped_bone() {
    printf("Test: Load config with unmapped bone... ");

    const char* config_content = R"({
        "mappings": [
            {"target": "mixamorig:LeftHandIndex1", "source": null}
        ]
    })";

    std::string config_path = "/tmp/test_unmapped.json";
    {
        std::ofstream file(config_path);
        file << config_content;
    }

    retarget::BoneMapper mapper;
    assert(mapper.LoadConfig(config_path));

    auto mapping = mapper.GetMapping("mixamorig:LeftHandIndex1");
    assert(mapping.has_value());
    assert(mapping->is_unmapped());
    assert(mapping->source_bones.empty());

    printf("PASSED\n");
}

void test_bone_name_lookup() {
    printf("Test: Bone name to index lookup... ");

    retarget::BoneMapper mapper;

    std::vector<std::string> source_names = {"model_root", "pelvis", "lfemurYZ", "lfemurX", "ltibia"};
    std::vector<std::string> target_names = {"mixamorig:Hips", "mixamorig:Spine", "mixamorig:LeftUpLeg"};

    mapper.BuildSourceBoneMap(source_names);
    mapper.BuildTargetBoneMap(target_names);

    assert(mapper.GetSourceBoneIndex("pelvis") == 1);
    assert(mapper.GetSourceBoneIndex("lfemurYZ") == 2);
    assert(mapper.GetSourceBoneIndex("nonexistent") == -1);

    assert(mapper.GetTargetBoneIndex("mixamorig:Hips") == 0);
    assert(mapper.GetTargetBoneIndex("mixamorig:LeftUpLeg") == 2);
    assert(mapper.GetTargetBoneIndex("nonexistent") == -1);

    printf("PASSED\n");
}

// ============================================================================
// Transform Tests
// ============================================================================

void test_quaternion_identity() {
    printf("Test: Identity quaternion... ");

    ozz::math::Quaternion identity = ozz::math::Quaternion::identity();
    assert(float_eq(identity.x, 0.0f));
    assert(float_eq(identity.y, 0.0f));
    assert(float_eq(identity.z, 0.0f));
    assert(float_eq(identity.w, 1.0f));

    printf("PASSED\n");
}

void test_combine_identity_rotations() {
    printf("Test: Combine identity rotations... ");

    std::vector<ozz::math::Quaternion> rotations = {
        ozz::math::Quaternion::identity(),
        ozz::math::Quaternion::identity()
    };

    auto result = retarget::BoneMapper::CombineRotations(rotations);
    assert(quat_eq(result, ozz::math::Quaternion::identity()));

    printf("PASSED\n");
}

void test_combine_single_rotation() {
    printf("Test: Combine single rotation... ");

    // 90 degree rotation around Y axis
    ozz::math::Quaternion rot(0.0f, 0.7071f, 0.0f, 0.7071f);
    std::vector<ozz::math::Quaternion> rotations = {rot};

    auto result = retarget::BoneMapper::CombineRotations(rotations);
    assert(quat_eq(result, rot, 0.01f));

    printf("PASSED\n");
}

void test_combine_two_rotations() {
    printf("Test: Combine two rotations... ");

    // Two 90-degree rotations around Y should give 180-degree rotation
    ozz::math::Quaternion rot90y(0.0f, 0.7071f, 0.0f, 0.7071f);
    std::vector<ozz::math::Quaternion> rotations = {rot90y, rot90y};

    auto result = retarget::BoneMapper::CombineRotations(rotations);

    // 180 degrees around Y: (0, 1, 0, 0) or (0, -1, 0, 0)
    ozz::math::Quaternion expected(0.0f, 1.0f, 0.0f, 0.0f);
    assert(quat_eq(result, expected, 0.01f));

    printf("PASSED\n");
}

void test_rotation_normalization() {
    printf("Test: Rotation normalization after combine... ");

    ozz::math::Quaternion rot(0.5f, 0.5f, 0.5f, 0.5f);
    std::vector<ozz::math::Quaternion> rotations = {rot, rot};

    auto result = retarget::BoneMapper::CombineRotations(rotations);

    float len = std::sqrt(result.x * result.x + result.y * result.y +
                          result.z * result.z + result.w * result.w);
    assert(float_eq(len, 1.0f, 0.001f));

    printf("PASSED\n");
}

// ============================================================================
// Integration Tests (require real files)
// ============================================================================

bool LoadSkeleton(const std::string& path, ozz::animation::Skeleton* skeleton) {
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened()) return false;

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Skeleton>()) return false;

    archive >> *skeleton;
    return true;
}

bool LoadAnimation(const std::string& path, ozz::animation::Animation* animation) {
    ozz::io::File file(path.c_str(), "rb");
    if (!file.opened()) return false;

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Animation>()) return false;

    archive >> *animation;
    return true;
}

void test_load_jka_skeleton() {
    printf("Test: Load JKA skeleton... ");

    ozz::animation::Skeleton skeleton;
    const char* path = "models/player/jka/humanoid.ozz";

    if (!LoadSkeleton(path, &skeleton)) {
        printf("SKIPPED (file not found: %s)\n", path);
        return;
    }

    assert(skeleton.num_joints() > 0);
    printf("joints=%d ", skeleton.num_joints());

    // Check for known bone names
    bool has_pelvis = false;
    for (int i = 0; i < skeleton.num_joints(); ++i) {
        if (std::string(skeleton.joint_names()[i]) == "pelvis") {
            has_pelvis = true;
            break;
        }
    }
    assert(has_pelvis);

    printf("PASSED\n");
}

void test_load_mixamo_skeleton() {
    printf("Test: Load Mixamo skeleton... ");

    ozz::animation::Skeleton skeleton;
    const char* path = "models/player/mixamo_skeleton.ozz";

    if (!LoadSkeleton(path, &skeleton)) {
        printf("SKIPPED (file not found: %s)\n", path);
        return;
    }

    assert(skeleton.num_joints() > 0);
    printf("joints=%d ", skeleton.num_joints());

    // Check for known bone names
    bool has_hips = false;
    for (int i = 0; i < skeleton.num_joints(); ++i) {
        if (std::string(skeleton.joint_names()[i]) == "mixamorig:Hips") {
            has_hips = true;
            break;
        }
    }
    assert(has_hips);

    printf("PASSED\n");
}

void test_load_jka_animation() {
    printf("Test: Load JKA animation... ");

    ozz::animation::Animation animation;
    const char* path = "models/player/jka/stand1.ozz";

    if (!LoadAnimation(path, &animation)) {
        printf("SKIPPED (file not found: %s)\n", path);
        return;
    }

    assert(animation.duration() > 0.0f);
    assert(animation.num_tracks() > 0);
    printf("duration=%.2fs tracks=%d ", animation.duration(), animation.num_tracks());

    printf("PASSED\n");
}

void test_skeleton_bone_counts() {
    printf("Test: Skeleton bone counts... ");

    ozz::animation::Skeleton jka_skeleton;
    ozz::animation::Skeleton mixamo_skeleton;

    if (!LoadSkeleton("models/player/jka/humanoid.ozz", &jka_skeleton)) {
        printf("SKIPPED (JKA skeleton not found)\n");
        return;
    }

    if (!LoadSkeleton("models/player/mixamo_skeleton.ozz", &mixamo_skeleton)) {
        printf("SKIPPED (Mixamo skeleton not found)\n");
        return;
    }

    printf("JKA=%d Mixamo=%d ", jka_skeleton.num_joints(), mixamo_skeleton.num_joints());

    // JKA should have ~53 joints, Mixamo should have ~65-66
    assert(jka_skeleton.num_joints() >= 50);
    assert(mixamo_skeleton.num_joints() >= 60);

    printf("PASSED\n");
}

void test_compute_world_positions() {
    printf("Test: Compute world positions from skeleton... ");

    ozz::animation::Skeleton skeleton;
    if (!LoadSkeleton("models/player/jka/humanoid.ozz", &skeleton)) {
        printf("SKIPPED (skeleton not found)\n");
        return;
    }

    int num_joints = skeleton.num_joints();
    int num_soa_joints = skeleton.num_soa_joints();

    // Use rest pose
    std::vector<ozz::math::SoaTransform> locals(num_soa_joints);
    for (int i = 0; i < num_soa_joints; ++i) {
        locals[i] = skeleton.joint_rest_poses()[i];
    }

    // Compute world matrices
    std::vector<ozz::math::Float4x4> models(num_joints);

    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton;
    ltm_job.input = ozz::make_span(locals);
    ltm_job.output = ozz::make_span(models);
    assert(ltm_job.Run());

    // Check that pelvis is above ground (positive Y in ozz Y-up)
    int pelvis_idx = -1;
    for (int i = 0; i < num_joints; ++i) {
        if (std::string(skeleton.joint_names()[i]) == "pelvis") {
            pelvis_idx = i;
            break;
        }
    }
    assert(pelvis_idx >= 0);

    float pelvis_y = ozz::math::GetY(models[pelvis_idx].cols[3]);
    printf("pelvis_y=%.2f ", pelvis_y);
    assert(pelvis_y > 0.0f);  // Should be above ground

    printf("PASSED\n");
}

// ============================================================================
// Position Verification Tests
// ============================================================================

void test_humanoid_proportions() {
    printf("Test: Humanoid proportions preserved... ");

    ozz::animation::Skeleton skeleton;
    if (!LoadSkeleton("models/player/jka/humanoid.ozz", &skeleton)) {
        printf("SKIPPED (skeleton not found)\n");
        return;
    }

    int num_joints = skeleton.num_joints();
    int num_soa_joints = skeleton.num_soa_joints();

    // Use rest pose
    std::vector<ozz::math::SoaTransform> locals(num_soa_joints);
    for (int i = 0; i < num_soa_joints; ++i) {
        locals[i] = skeleton.joint_rest_poses()[i];
    }

    std::vector<ozz::math::Float4x4> models(num_joints);

    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton;
    ltm_job.input = ozz::make_span(locals);
    ltm_job.output = ozz::make_span(models);
    assert(ltm_job.Run());

    // Find key bones
    int pelvis_idx = -1, cranium_idx = -1, ltalus_idx = -1;
    for (int i = 0; i < num_joints; ++i) {
        std::string name = skeleton.joint_names()[i];
        if (name == "pelvis") pelvis_idx = i;
        else if (name == "cranium") cranium_idx = i;
        else if (name == "ltalus") ltalus_idx = i;
    }

    if (pelvis_idx < 0 || cranium_idx < 0 || ltalus_idx < 0) {
        printf("SKIPPED (key bones not found)\n");
        return;
    }

    float pelvis_y = ozz::math::GetY(models[pelvis_idx].cols[3]);
    float head_y = ozz::math::GetY(models[cranium_idx].cols[3]);
    float foot_y = ozz::math::GetY(models[ltalus_idx].cols[3]);

    printf("head=%.2f pelvis=%.2f foot=%.2f ", head_y, pelvis_y, foot_y);

    // Head should be above pelvis
    assert(head_y > pelvis_y);

    // Pelvis should be above foot
    assert(pelvis_y > foot_y);

    printf("PASSED\n");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=== ozz-retarget Unit Tests ===\n\n");

    // Bone Mapper Tests
    printf("--- Bone Mapper Tests ---\n");
    test_load_config_single_bone();
    test_load_config_combined_bones();
    test_load_config_unmapped_bone();
    test_bone_name_lookup();

    // Transform Tests
    printf("\n--- Transform Tests ---\n");
    test_quaternion_identity();
    test_combine_identity_rotations();
    test_combine_single_rotation();
    test_combine_two_rotations();
    test_rotation_normalization();

    // Integration Tests
    printf("\n--- Integration Tests ---\n");
    test_load_jka_skeleton();
    test_load_mixamo_skeleton();
    test_load_jka_animation();
    test_skeleton_bone_counts();
    test_compute_world_positions();

    // Position Tests
    printf("\n--- Position Verification Tests ---\n");
    test_humanoid_proportions();

    printf("\n=== All Tests Complete ===\n");
    return 0;
}
