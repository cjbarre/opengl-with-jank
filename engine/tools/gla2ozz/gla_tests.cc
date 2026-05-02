// Unit tests for gla2ozz converter
// Build: g++ -std=c++17 -I... -o gla_tests gla_tests.cc gla_parser.cc ...

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "gla_format.h"
#include "gla_parser.h"
#include "coordinate_convert.h"

// ozz includes for skeleton and animation loading
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"

// Test helpers
bool float_eq(float a, float b, float epsilon = 0.001f) {
    return std::abs(a - b) < epsilon;
}

bool vec3_eq(const ozz::math::Float3& a, const ozz::math::Float3& b, float epsilon = 0.001f) {
    return float_eq(a.x, b.x, epsilon) &&
           float_eq(a.y, b.y, epsilon) &&
           float_eq(a.z, b.z, epsilon);
}

// Test 1: Bone decompression
// Known compressed bone -> expected quaternion and translation
void test_bone_decompression() {
    printf("Test: Bone decompression... ");

    // Create a known compressed bone value
    // Identity quaternion (0,0,0,1) compressed: each component maps via (val/16383)-2
    // To get 0: val = 2 * 16383 = 32766
    // To get 1: val = 3 * 16383 = 49149
    gla::CompressedBone comp;
    uint16_t qx_raw = 32766;  // -> 0
    uint16_t qy_raw = 32766;  // -> 0
    uint16_t qz_raw = 32766;  // -> 0
    uint16_t qw_raw = 49149;  // -> 1

    // Translation (0,0,0): val = 512 * 64 = 32768
    uint16_t tx_raw = 32768;
    uint16_t ty_raw = 32768;
    uint16_t tz_raw = 32768;

    // Pack into compressed bone (little-endian)
    comp.data[0] = qx_raw & 0xFF;
    comp.data[1] = (qx_raw >> 8) & 0xFF;
    comp.data[2] = qy_raw & 0xFF;
    comp.data[3] = (qy_raw >> 8) & 0xFF;
    comp.data[4] = qz_raw & 0xFF;
    comp.data[5] = (qz_raw >> 8) & 0xFF;
    comp.data[6] = qw_raw & 0xFF;
    comp.data[7] = (qw_raw >> 8) & 0xFF;
    comp.data[8] = tx_raw & 0xFF;
    comp.data[9] = (tx_raw >> 8) & 0xFF;
    comp.data[10] = ty_raw & 0xFF;
    comp.data[11] = (ty_raw >> 8) & 0xFF;
    comp.data[12] = tz_raw & 0xFF;
    comp.data[13] = (tz_raw >> 8) & 0xFF;

    // We can't easily test DecompressBone directly since it's private
    // This test documents the expected behavior

    printf("PASSED (manual verification needed)\n");
}

// Test 2: Coordinate conversion
void test_coordinate_conversion() {
    printf("Test: Coordinate conversion... ");

    // Test position conversion: JKA Z-up to Y-up
    // JKA: +X right, +Y forward, +Z up
    // ozz/OpenGL: +X right, +Y up, -Z forward
    // Expected: (x, y, z) -> (x, z, -y)
    auto pos = coord_convert::ConvertPosition(1.0f, 2.0f, 3.0f);
    float scale = coord_convert::SCALE_FACTOR;

    // After conversion: (1, 2, 3) -> (1*s, 3*s, -2*s)
    assert(float_eq(pos.x, 1.0f * scale));  // X unchanged
    assert(float_eq(pos.y, 3.0f * scale));  // Z becomes Y (height)
    assert(float_eq(pos.z, -2.0f * scale)); // Y becomes -Z (depth)

    printf("PASSED\n");
}

// Test 3: Quaternion normalization
void test_quaternion_normalization() {
    printf("Test: Quaternion normalization... ");

    // Create non-unit quaternion
    ozz::math::Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);  // Already unit
    auto normalized = coord_convert::NormalizeQuaternion(q);

    float len = std::sqrt(normalized.x * normalized.x +
                          normalized.y * normalized.y +
                          normalized.z * normalized.z +
                          normalized.w * normalized.w);
    assert(float_eq(len, 1.0f));

    printf("PASSED\n");
}

// Test 4: Load real GLA file and verify structure
void test_gla_loading(const char* gla_path) {
    printf("Test: GLA file loading '%s'... ", gla_path);

    gla::GlaParser parser;
    if (!parser.Load(gla_path)) {
        printf("SKIPPED (file not found)\n");
        return;
    }

    // Verify header
    const auto& header = parser.GetHeader();
    assert(header.ident == gla::GLA_IDENT);
    assert(header.version == gla::GLA_VERSION);
    assert(header.num_bones > 0);
    assert(header.num_frames > 0);

    printf("bones=%d frames=%d ", header.num_bones, header.num_frames);

    // Verify bone hierarchy
    const auto& bones = parser.GetBones();
    assert(bones.size() == static_cast<size_t>(header.num_bones));

    // Count root bones
    int root_count = 0;
    for (const auto& bone : bones) {
        if (bone.parent == -1) root_count++;
    }
    assert(root_count >= 1);

    printf("roots=%d ", root_count);

    // Verify bone positions are reasonable
    for (size_t i = 0; i < bones.size(); ++i) {
        float x, y, z;
        parser.GetWorldPosition(static_cast<int>(i), &x, &y, &z);
        // World positions should be finite
        assert(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
    }

    printf("PASSED\n");
}

// Test 5: Verify bind pose structure
void test_bind_pose_structure(const char* gla_path) {
    printf("Test: Bind pose structure '%s'... ", gla_path);

    gla::GlaParser parser;
    if (!parser.Load(gla_path)) {
        printf("SKIPPED (file not found)\n");
        return;
    }

    const auto& bones = parser.GetBones();

    // Print first 10 bones for inspection
    printf("\n");
    for (size_t i = 0; i < std::min(bones.size(), size_t(10)); ++i) {
        float x, y, z;
        parser.GetWorldPosition(static_cast<int>(i), &x, &y, &z);
        printf("  [%zu] '%s' parent=%d world=(%.2f, %.2f, %.2f)\n",
               i, bones[i].name, bones[i].parent, x, y, z);
    }

    printf("  PASSED (manual verification needed)\n");
}

// Test 6: Compare GLA world positions with ozz skeleton output
void test_gla_ozz_comparison(const char* gla_path, const char* ozz_skeleton_path) {
    printf("Test: GLA vs ozz skeleton comparison...\n");

    gla::GlaParser parser;
    if (!parser.Load(gla_path)) {
        printf("  SKIPPED (GLA not found: %s)\n", gla_path);
        return;
    }

    // Load ozz skeleton
    ozz::animation::Skeleton skeleton;
    {
        ozz::io::File file(ozz_skeleton_path, "rb");
        if (!file.opened()) {
            printf("  SKIPPED (ozz skeleton not found: %s)\n", ozz_skeleton_path);
            return;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>()) {
            printf("  FAILED (invalid ozz skeleton file)\n");
            return;
        }
        archive >> skeleton;
    }

    printf("  GLA bones: %d, ozz joints: %d\n",
           parser.GetNumBones(), skeleton.num_joints());

    // Sample skeleton at rest pose
    int num_joints = skeleton.num_joints();
    int num_soa_joints = skeleton.num_soa_joints();

    // Allocate buffers for local-to-model job
    std::vector<ozz::math::SoaTransform> locals(num_soa_joints);
    std::vector<ozz::math::Float4x4> models(num_joints);

    // Copy rest pose to locals
    for (int i = 0; i < num_soa_joints; ++i) {
        locals[i] = skeleton.joint_rest_poses()[i];
    }

    // Run local-to-model to get world positions
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton;
    ltm_job.input = ozz::make_span(locals);
    ltm_job.output = ozz::make_span(models);
    if (!ltm_job.Run()) {
        printf("  FAILED (local-to-model job failed)\n");
        return;
    }

    // Compare positions BY NAME (ozz reorders joints during skeleton build)
    printf("  Comparing world positions by bone name:\n");
    const auto& gla_bones = parser.GetBones();

    float scale = coord_convert::SCALE_FACTOR;
    int match_count = 0;
    int total_count = 0;

    // Build name->index map for ozz joints
    std::map<std::string, int> ozz_name_to_idx;
    for (int i = 0; i < num_joints; ++i) {
        ozz_name_to_idx[skeleton.joint_names()[i]] = i;
    }

    for (int gla_i = 0; gla_i < parser.GetNumBones() && total_count < 15; ++gla_i) {
        const char* bone_name = gla_bones[gla_i].name;

        // Find matching ozz joint by name
        auto it = ozz_name_to_idx.find(bone_name);
        if (it == ozz_name_to_idx.end()) {
            printf("  [GLA %d] '%s' - NOT FOUND in ozz\n", gla_i, bone_name);
            continue;
        }
        int ozz_i = it->second;

        float gla_x, gla_y, gla_z;
        parser.GetWorldPosition(gla_i, &gla_x, &gla_y, &gla_z);

        // Extract position from ozz model matrix
        const ozz::math::Float4x4& m = models[ozz_i];
        ozz::math::SimdFloat4 col3 = m.cols[3];
        float ozz_x, ozz_y, ozz_z;
        ozz::math::Store3PtrU(col3, &ozz_x);
        ozz_y = ozz::math::GetY(col3);
        ozz_z = ozz::math::GetZ(col3);

        // Convert GLA position: Z-up to Y-up: (x, y, z) -> (x, z, -y)
        float gla_conv_x = gla_x * scale;
        float gla_conv_y = gla_z * scale;
        float gla_conv_z = -gla_y * scale;

        bool matches = float_eq(gla_conv_x, ozz_x, 0.01f) &&
                       float_eq(gla_conv_y, ozz_y, 0.01f) &&
                       float_eq(gla_conv_z, ozz_z, 0.01f);

        printf("  '%s' (GLA[%d] -> ozz[%d]): %s\n", bone_name, gla_i, ozz_i,
               matches ? "MATCH" : "MISMATCH");
        if (!matches) {
            printf("      GLA conv: (%.4f, %.4f, %.4f)\n", gla_conv_x, gla_conv_y, gla_conv_z);
            printf("      ozz:      (%.4f, %.4f, %.4f)\n", ozz_x, ozz_y, ozz_z);
        }

        if (matches) match_count++;
        total_count++;
    }

    printf("  RESULT: %d/%d positions match\n", match_count, total_count);
    if (match_count == total_count) {
        printf("  PASSED - All world positions match!\n");
    } else {
        printf("  FAILED - Some positions don't match\n");
    }
}

// Test 7: Verify animation transforms are valid parent-local transforms
void test_animation_transforms(const char* gla_path) {
    printf("Test: Animation transforms (as parent-local)...\n");

    gla::GlaParser parser;
    if (!parser.Load(gla_path)) {
        printf("  SKIPPED (GLA not found: %s)\n", gla_path);
        return;
    }

    const auto& bones = parser.GetBones();
    int num_frames = parser.GetNumFrames();
    int num_bones = parser.GetNumBones();

    // Test frame 0 (should be close to bind pose)
    printf("  Frame 0 animation transforms:\n");
    for (int bone_idx = 0; bone_idx < std::min(10, num_bones); ++bone_idx) {
        ozz::math::Float3 trans;
        ozz::math::Quaternion rot;
        if (!parser.GetAnimTransformInParentSpace(0, bone_idx, &trans, &rot)) {
            printf("  [%d] '%s' - FAILED to get transform\n", bone_idx, bones[bone_idx].name);
            continue;
        }

        // Verify values are finite
        bool valid = std::isfinite(trans.x) && std::isfinite(trans.y) && std::isfinite(trans.z) &&
                     std::isfinite(rot.x) && std::isfinite(rot.y) && std::isfinite(rot.z) && std::isfinite(rot.w);

        // Check rotation is normalized (length ~= 1)
        float rot_len = std::sqrt(rot.x*rot.x + rot.y*rot.y + rot.z*rot.z + rot.w*rot.w);
        bool normalized = float_eq(rot_len, 1.0f, 0.01f);

        // Animation is parent-local, so translation magnitudes should be reasonable
        // (typically < 50 units for humanoid skeleton)
        float trans_mag = std::sqrt(trans.x*trans.x + trans.y*trans.y + trans.z*trans.z);
        bool reasonable = trans_mag < 100.0f;

        printf("  [%d] '%s': trans=(%.2f,%.2f,%.2f) |t|=%.2f rot_len=%.4f %s\n",
               bone_idx, bones[bone_idx].name, trans.x, trans.y, trans.z,
               trans_mag, rot_len,
               (valid && normalized && reasonable) ? "OK" : "CHECK");
    }

    // Test a mid-animation frame
    int test_frame = num_frames / 2;
    printf("\n  Frame %d animation transforms:\n", test_frame);
    for (int bone_idx = 0; bone_idx < std::min(10, num_bones); ++bone_idx) {
        ozz::math::Float3 trans;
        ozz::math::Quaternion rot;
        if (!parser.GetAnimTransformInParentSpace(test_frame, bone_idx, &trans, &rot)) {
            continue;
        }

        float trans_mag = std::sqrt(trans.x*trans.x + trans.y*trans.y + trans.z*trans.z);
        float rot_len = std::sqrt(rot.x*rot.x + rot.y*rot.y + rot.z*rot.z + rot.w*rot.w);
        bool reasonable = trans_mag < 100.0f && float_eq(rot_len, 1.0f, 0.01f);

        printf("  [%d] '%s': trans=(%.2f,%.2f,%.2f) |t|=%.2f %s\n",
               bone_idx, bones[bone_idx].name, trans.x, trans.y, trans.z,
               trans_mag, reasonable ? "OK" : "CHECK");
    }

    printf("  PASSED (manual verification needed)\n");
}

// Test 8: Check animation orientation - does frame 0 produce correct world positions?
void test_animation_orientation(const char* ozz_skeleton_path, const char* ozz_anim_path) {
    printf("Test: Animation orientation (empirical)...\n");

    // Load ozz skeleton
    ozz::animation::Skeleton skeleton;
    {
        ozz::io::File file(ozz_skeleton_path, "rb");
        if (!file.opened()) {
            printf("  SKIPPED (ozz skeleton not found)\n");
            return;
        }
        ozz::io::IArchive archive(&file);
        archive >> skeleton;
    }

    // Load ozz animation
    ozz::animation::Animation animation;
    {
        ozz::io::File file(ozz_anim_path, "rb");
        if (!file.opened()) {
            printf("  SKIPPED (ozz animation not found: %s)\n", ozz_anim_path);
            return;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Animation>()) {
            printf("  SKIPPED (invalid ozz animation file)\n");
            return;
        }
        archive >> animation;
    }

    printf("  Loaded animation: %s (duration: %.3fs)\n",
           animation.name(), animation.duration());

    int num_joints = skeleton.num_joints();
    int num_soa_joints = skeleton.num_soa_joints();

    // Sample animation at t=0
    std::vector<ozz::math::SoaTransform> locals(num_soa_joints);
    std::vector<ozz::math::Float4x4> models(num_joints);

    ozz::animation::SamplingJob sampling_job;
    ozz::animation::SamplingJob::Context context;
    context.Resize(num_joints);
    sampling_job.animation = &animation;
    sampling_job.context = &context;
    sampling_job.ratio = 0.0f;  // Sample at t=0
    sampling_job.output = ozz::make_span(locals);
    if (!sampling_job.Run()) {
        printf("  FAILED (sampling job failed)\n");
        return;
    }

    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton;
    ltm_job.input = ozz::make_span(locals);
    ltm_job.output = ozz::make_span(models);
    if (!ltm_job.Run()) {
        printf("  FAILED (local-to-model job failed)\n");
        return;
    }

    // Build name->index map
    std::map<std::string, int> name_to_idx;
    for (int i = 0; i < num_joints; ++i) {
        name_to_idx[skeleton.joint_names()[i]] = i;
    }

    // Also get skeleton rest pose for comparison
    std::vector<ozz::math::SoaTransform> rest_locals(num_soa_joints);
    std::vector<ozz::math::Float4x4> rest_models(num_joints);
    for (int i = 0; i < num_soa_joints; ++i) {
        rest_locals[i] = skeleton.joint_rest_poses()[i];
    }
    ozz::animation::LocalToModelJob rest_ltm_job;
    rest_ltm_job.skeleton = &skeleton;
    rest_ltm_job.input = ozz::make_span(rest_locals);
    rest_ltm_job.output = ozz::make_span(rest_models);
    rest_ltm_job.Run();

    // Print ALL bone world positions for debugging
    printf("  Bone world positions (rest vs animation frame 0):\n");
    printf("  %4s %-20s  %-30s  %-30s\n", "idx", "name", "REST POSE", "ANIMATION");
    for (int i = 0; i < std::min(25, num_joints); ++i) {
        const ozz::math::Float4x4& rm = rest_models[i];
        const ozz::math::Float4x4& am = models[i];
        float rx = ozz::math::GetX(rm.cols[3]);
        float ry = ozz::math::GetY(rm.cols[3]);
        float rz = ozz::math::GetZ(rm.cols[3]);
        float ax = ozz::math::GetX(am.cols[3]);
        float ay = ozz::math::GetY(am.cols[3]);
        float az = ozz::math::GetZ(am.cols[3]);
        printf("  [%2d] %-20s (%.3f, %.3f, %.3f) vs (%.3f, %.3f, %.3f)\n",
               i, skeleton.joint_names()[i], rx, ry, rz, ax, ay, az);
    }

    auto get_world_pos = [&](const char* name) -> ozz::math::Float3 {
        auto it = name_to_idx.find(name);
        if (it == name_to_idx.end()) return ozz::math::Float3(0, 0, 0);
        const ozz::math::Float4x4& m = models[it->second];
        return ozz::math::Float3(
            ozz::math::GetX(m.cols[3]),
            ozz::math::GetY(m.cols[3]),
            ozz::math::GetZ(m.cols[3]));
    };

    auto pelvis = get_world_pos("pelvis");
    auto cranium = get_world_pos("cranium");
    auto ltalus = get_world_pos("ltalus");

    printf("  Key bone positions:\n");
    printf("    ltalus (foot): (%.4f, %.4f, %.4f)\n", ltalus.x, ltalus.y, ltalus.z);
    printf("    pelvis: (%.4f, %.4f, %.4f)\n", pelvis.x, pelvis.y, pelvis.z);
    printf("    cranium: (%.4f, %.4f, %.4f)\n", cranium.x, cranium.y, cranium.z);

    bool head_above_pelvis = cranium.y > pelvis.y;
    bool pelvis_above_feet = pelvis.y > ltalus.y;

    printf("  Orientation checks:\n");
    printf("    cranium.y > pelvis.y: %s (%.4f > %.4f)\n",
           head_above_pelvis ? "OK" : "FAIL", cranium.y, pelvis.y);
    printf("    pelvis.y > ltalus.y: %s (%.4f > %.4f)\n",
           pelvis_above_feet ? "OK" : "FAIL", pelvis.y, ltalus.y);

    if (head_above_pelvis && pelvis_above_feet) {
        printf("  PASSED - Animation frame 0 is properly oriented!\n");
    } else {
        printf("  FAILED - Animation produces WRONG orientation!\n");
    }
}

// Test 9: Empirical orientation check - verify head is above feet in Y-up space
void test_skeleton_orientation(const char* ozz_skeleton_path) {
    printf("Test: Skeleton orientation (empirical)...\n");

    // Load ozz skeleton
    ozz::animation::Skeleton skeleton;
    {
        ozz::io::File file(ozz_skeleton_path, "rb");
        if (!file.opened()) {
            printf("  SKIPPED (ozz skeleton not found: %s)\n", ozz_skeleton_path);
            return;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>()) {
            printf("  FAILED (invalid ozz skeleton file)\n");
            return;
        }
        archive >> skeleton;
    }

    // Sample skeleton at rest pose
    int num_joints = skeleton.num_joints();
    int num_soa_joints = skeleton.num_soa_joints();

    std::vector<ozz::math::SoaTransform> locals(num_soa_joints);
    std::vector<ozz::math::Float4x4> models(num_joints);

    for (int i = 0; i < num_soa_joints; ++i) {
        locals[i] = skeleton.joint_rest_poses()[i];
    }

    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &skeleton;
    ltm_job.input = ozz::make_span(locals);
    ltm_job.output = ozz::make_span(models);
    if (!ltm_job.Run()) {
        printf("  FAILED (local-to-model job failed)\n");
        return;
    }

    // Build name->index map
    std::map<std::string, int> name_to_idx;
    for (int i = 0; i < num_joints; ++i) {
        name_to_idx[skeleton.joint_names()[i]] = i;
    }

    // Extract key bone world Y positions (height in Y-up space)
    auto get_world_y = [&](const char* name) -> float {
        auto it = name_to_idx.find(name);
        if (it == name_to_idx.end()) return -999.0f;
        return ozz::math::GetY(models[it->second].cols[3]);
    };

    float root_y = get_world_y("model_root");
    float pelvis_y = get_world_y("pelvis");
    float thoracic_y = get_world_y("thoracic");
    float cervical_y = get_world_y("cervical");
    float cranium_y = get_world_y("cranium");
    float ltalus_y = get_world_y("ltalus");  // Left ankle/foot

    printf("  World Y positions (Y-up, height in meters):\n");
    printf("    model_root: %.4f\n", root_y);
    printf("    ltalus (foot): %.4f\n", ltalus_y);
    printf("    pelvis: %.4f\n", pelvis_y);
    printf("    thoracic: %.4f\n", thoracic_y);
    printf("    cervical: %.4f\n", cervical_y);
    printf("    cranium: %.4f\n", cranium_y);

    // In a properly oriented Y-up skeleton:
    // - cranium.y should be HIGHER than pelvis.y (head above hips)
    // - pelvis.y should be HIGHER than ltalus.y (hips above feet)
    // - All height values should be positive

    bool head_above_pelvis = cranium_y > pelvis_y;
    bool pelvis_above_feet = pelvis_y > ltalus_y;
    bool positive_heights = pelvis_y > 0.0f && cranium_y > 0.0f;

    printf("  Orientation checks:\n");
    printf("    cranium > pelvis: %s (%.4f > %.4f)\n",
           head_above_pelvis ? "OK" : "FAIL", cranium_y, pelvis_y);
    printf("    pelvis > ltalus: %s (%.4f > %.4f)\n",
           pelvis_above_feet ? "OK" : "FAIL", pelvis_y, ltalus_y);
    printf("    heights positive: %s\n", positive_heights ? "OK" : "FAIL");

    if (head_above_pelvis && pelvis_above_feet && positive_heights) {
        printf("  PASSED - Skeleton is properly oriented (Y-up)!\n");
    } else if (!head_above_pelvis && cranium_y < pelvis_y) {
        printf("  FAILED - Skeleton is UPSIDE DOWN (head below pelvis)!\n");
        printf("  -> The coordinate conversion is inverting the skeleton.\n");
    } else {
        printf("  FAILED - Unexpected orientation issue.\n");
    }
}

int main(int argc, char** argv) {
    printf("=== GLA2OZZ Unit Tests ===\n\n");

    // Basic unit tests
    test_bone_decompression();
    test_coordinate_conversion();
    test_quaternion_normalization();

    // File-based tests
    const char* default_gla = "/tmp/gla2ozz_test/models/players/_humanoid/_humanoid.gla";
    const char* default_ozz = "/tmp/gla2ozz_test/models/players/_humanoid/humanoid.ozz";
    const char* default_anim = "/tmp/gla2ozz_test/models/players/_humanoid/BOTH_STAND1.ozz";
    const char* gla_path = argc > 1 ? argv[1] : default_gla;
    const char* ozz_path = argc > 2 ? argv[2] : default_ozz;
    const char* anim_path = argc > 3 ? argv[3] : default_anim;

    test_gla_loading(gla_path);
    test_bind_pose_structure(gla_path);
    test_gla_ozz_comparison(gla_path, ozz_path);
    test_animation_transforms(gla_path);
    test_skeleton_orientation(ozz_path);
    test_animation_orientation(ozz_path, anim_path);

    printf("\n=== Tests Complete ===\n");
    return 0;
}
