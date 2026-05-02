#pragma once
#include "animation_types.h"
#include "ozz_mesh.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/math_archive.h"
#include "ozz/base/maths/simd_math_archive.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/containers/vector_archive.h"
#include <iostream>
#include <fstream>
#include <cstring>

namespace eanim {
// Factory function to avoid copy construction issues
inline AnimationContext* create_animation_context() {
  return new AnimationContext();
}
} // namespace eanim

// Mesh archive implementation (from ozz samples framework)
namespace ozz {
namespace io {

inline void Extern<sample::Mesh::Part>::Save(OArchive& _archive,
                                      const sample::Mesh::Part* _parts,
                                      size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const sample::Mesh::Part& part = _parts[i];
    _archive << part.positions;
    _archive << part.normals;
    _archive << part.tangents;
    _archive << part.uvs;
    _archive << part.colors;
    _archive << part.joint_indices;
    _archive << part.joint_weights;
  }
}

inline void Extern<sample::Mesh::Part>::Load(IArchive& _archive,
                                      sample::Mesh::Part* _parts, size_t _count,
                                      uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    sample::Mesh::Part& part = _parts[i];
    _archive >> part.positions;
    _archive >> part.normals;
    _archive >> part.tangents;
    _archive >> part.uvs;
    _archive >> part.colors;
    _archive >> part.joint_indices;
    _archive >> part.joint_weights;
  }
}

inline void Extern<sample::Mesh>::Save(OArchive& _archive, const sample::Mesh* _meshes,
                                size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const sample::Mesh& mesh = _meshes[i];
    _archive << mesh.parts;
    _archive << mesh.triangle_indices;
    _archive << mesh.joint_remaps;
    _archive << mesh.inverse_bind_poses;
  }
}

inline void Extern<sample::Mesh>::Load(IArchive& _archive, sample::Mesh* _meshes,
                                size_t _count, uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    sample::Mesh& mesh = _meshes[i];
    _archive >> mesh.parts;
    _archive >> mesh.triangle_indices;
    _archive >> mesh.joint_remaps;
    _archive >> mesh.inverse_bind_poses;
  }
}
}  // namespace io
}  // namespace ozz

// AnimationContext is defined in animation_types.h

namespace eanim {

// Load skeleton from .ozz file
inline ozz::animation::Skeleton* load_skeleton_ozz(const char* path) {
  ozz::io::File file(path, "rb");
  if (!file.opened()) {
    return nullptr;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Skeleton>()) {
    return nullptr;
  }
  ozz::animation::Skeleton* skeleton = new ozz::animation::Skeleton();
  archive >> *skeleton;
  return skeleton;
}

// Load animation from .ozz file
inline ozz::animation::Animation* load_animation_ozz(const char* path) {
  ozz::io::File file(path, "rb");
  if (!file.opened()) {
    return nullptr;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Animation>()) {
    return nullptr;
  }
  ozz::animation::Animation* animation = new ozz::animation::Animation();
  archive >> *animation;
  return animation;
}

// Sample animation at a given time ratio (0.0 to 1.0)
inline bool sample_animation_ozz(AnimationContext* ctx, ozz::animation::Animation* anim, float time_ratio) {
  if (!ctx || !anim || !ctx->skeleton) {
    return false;
  }

  // Sample animation
  ozz::animation::SamplingJob sampling_job;
  sampling_job.animation = anim;
  sampling_job.context = &ctx->context;
  sampling_job.ratio = time_ratio;
  sampling_job.output = ozz::make_span(ctx->locals);
  if (!sampling_job.Run()) {
    return false;
  }

  // Convert to model space
  ozz::animation::LocalToModelJob ltm_job;
  ltm_job.skeleton = ctx->skeleton;
  ltm_job.input = ozz::make_span(ctx->locals);
  ltm_job.output = ozz::make_span(ctx->models);
  if (!ltm_job.Run()) {
    return false;
  }

  return true;
}

// ============ MESH LOADING ============

// Load meshes from .ozz file
inline ozz::vector<ozz::sample::Mesh>* load_meshes_ozz(const char* path) {
  ozz::io::File file(path, "rb");
  if (!file.opened()) {
    return nullptr;
  }
  ozz::io::IArchive archive(&file);

  ozz::vector<ozz::sample::Mesh>* meshes = new ozz::vector<ozz::sample::Mesh>();
  while (archive.TestTag<ozz::sample::Mesh>()) {
    meshes->resize(meshes->size() + 1);
    archive >> meshes->back();
  }

  if (meshes->empty()) {
    delete meshes;
    return nullptr;
  }
  return meshes;
}

// Compute skinning matrices from model matrices and mesh
inline void compute_skinning_matrices(
  AnimationContext* ctx,
  ozz::vector<ozz::sample::Mesh>* meshes,
  int mesh_index,
  float* out_matrices  // Should have space for joint_remaps.size() * 16 floats
) {
  if (!ctx || !meshes || mesh_index < 0 || mesh_index >= static_cast<int>(meshes->size())) {
    return;
  }
  const auto& mesh = (*meshes)[mesh_index];
  for (size_t i = 0; i < mesh.joint_remaps.size(); ++i) {
    ozz::math::Float4x4 skinning_matrix =
        ctx->models[mesh.joint_remaps[i]] * mesh.inverse_bind_poses[i];
    memcpy(out_matrices + i * 16, &skinning_matrix, sizeof(float) * 16);
  }
}

// compute_and_upload_skinning_matrices is defined in animation_types.h

// ============ SKELETON DEBUG VISUALIZATION ============

// Get joint position from model matrix (translation column)
inline void get_joint_position(AnimationContext* ctx, int index, float* out_xyz) {
  if (!ctx || index < 0 || index >= static_cast<int>(ctx->models.size())) {
    out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
    return;
  }
  // ozz Float4x4 is column-major, translation is in column 3 (indices 12,13,14)
  const float* m = reinterpret_cast<const float*>(&ctx->models[index]);
  out_xyz[0] = m[12];
  out_xyz[1] = m[13];
  out_xyz[2] = m[14];
}

// Build skeleton line vertices for debug rendering
// Returns number of lines (pairs of vertices) written
// Each line is 2 vertices * 3 floats = 6 floats
inline int build_skeleton_lines(AnimationContext* ctx, float* out_vertices, int max_lines) {
  if (!ctx || !ctx->skeleton) return 0;

  int num_joints = ctx->skeleton->num_joints();
  auto parents = ctx->skeleton->joint_parents();
  int line_count = 0;

  for (int i = 0; i < num_joints && line_count < max_lines; ++i) {
    int parent = parents[i];
    if (parent < 0) continue;  // Root has no parent
    if (parent == 0) continue;  // Skip bones connected to root (model_root) - avoids vertical line to origin

    float child_pos[3], parent_pos[3];
    get_joint_position(ctx, i, child_pos);
    get_joint_position(ctx, parent, parent_pos);

    // Write line segment (2 vertices)
    int offset = line_count * 6;
    out_vertices[offset + 0] = parent_pos[0];
    out_vertices[offset + 1] = parent_pos[1];
    out_vertices[offset + 2] = parent_pos[2];
    out_vertices[offset + 3] = child_pos[0];
    out_vertices[offset + 4] = child_pos[1];
    out_vertices[offset + 5] = child_pos[2];
    ++line_count;
  }
  return line_count;
}

// Build skeleton lines from REST POSE (bind pose) - no animation
inline int build_skeleton_lines_rest_pose(AnimationContext* ctx, float* out_vertices, int max_lines) {
  if (!ctx || !ctx->skeleton) return 0;

  ozz::animation::Skeleton* skel = ctx->skeleton;
  int num_joints = skel->num_joints();
  int num_soa = skel->num_soa_joints();
  auto parents = skel->joint_parents();

  // Compute rest pose world positions
  std::vector<ozz::math::SoaTransform> rest_locals(num_soa);
  std::vector<ozz::math::Float4x4> rest_models(num_joints);

  for (int i = 0; i < num_soa; ++i) {
    rest_locals[i] = skel->joint_rest_poses()[i];
  }

  ozz::animation::LocalToModelJob ltm_job;
  ltm_job.skeleton = skel;
  ltm_job.input = ozz::make_span(rest_locals);
  ltm_job.output = ozz::make_span(rest_models);
  if (!ltm_job.Run()) return 0;

  // Build line segments
  int line_count = 0;
  for (int i = 0; i < num_joints && line_count < max_lines; ++i) {
    int parent = parents[i];
    if (parent < 0) continue;

    const ozz::math::Float4x4& child_m = rest_models[i];
    const ozz::math::Float4x4& parent_m = rest_models[parent];

    float cx = ozz::math::GetX(child_m.cols[3]);
    float cy = ozz::math::GetY(child_m.cols[3]);
    float cz = ozz::math::GetZ(child_m.cols[3]);
    float px = ozz::math::GetX(parent_m.cols[3]);
    float py = ozz::math::GetY(parent_m.cols[3]);
    float pz = ozz::math::GetZ(parent_m.cols[3]);

    int offset = line_count * 6;
    out_vertices[offset + 0] = px;
    out_vertices[offset + 1] = py;
    out_vertices[offset + 2] = pz;
    out_vertices[offset + 3] = cx;
    out_vertices[offset + 4] = cy;
    out_vertices[offset + 5] = cz;
    ++line_count;
  }
  return line_count;
}

// Build joint point vertices for debug rendering
// Returns number of points written (3 floats each)
inline int build_joint_points(AnimationContext* ctx, float* out_vertices, int max_points) {
  if (!ctx) return 0;
  int num_joints = static_cast<int>(ctx->models.size());
  int count = (num_joints < max_points) ? num_joints : max_points;

  for (int i = 0; i < count; ++i) {
    get_joint_position(ctx, i, out_vertices + i * 3);
  }
  return count;
}

// Debug: print first N joint positions
inline void debug_print_joint_positions(AnimationContext* ctx, int count) {
  if (!ctx || !ctx->skeleton) {
    printf("DEBUG: No skeleton\n");
    return;
  }
  int n = std::min(count, static_cast<int>(ctx->models.size()));
  printf("DEBUG: First %d joint positions:\n", n);
  for (int i = 0; i < n; ++i) {
    float pos[3];
    get_joint_position(ctx, i, pos);
    const char* name = ctx->skeleton->joint_names()[i];
    printf("  [%d] '%s': (%.4f, %.4f, %.4f)\n", i, name, pos[0], pos[1], pos[2]);
  }
}

// Debug: print REST POSE (bind pose) positions - independent of animation
inline void debug_print_rest_pose_positions(AnimationContext* ctx, int count) {
  if (!ctx || !ctx->skeleton) {
    printf("DEBUG REST POSE: No skeleton\n");
    return;
  }

  ozz::animation::Skeleton* skel = ctx->skeleton;
  int num_joints = skel->num_joints();
  int num_soa = skel->num_soa_joints();

  // Create temp buffers for rest pose computation
  std::vector<ozz::math::SoaTransform> rest_locals(num_soa);
  std::vector<ozz::math::Float4x4> rest_models(num_joints);

  // Copy rest poses
  for (int i = 0; i < num_soa; ++i) {
    rest_locals[i] = skel->joint_rest_poses()[i];
  }

  // Run LocalToModel to get world positions
  ozz::animation::LocalToModelJob ltm_job;
  ltm_job.skeleton = skel;
  ltm_job.input = ozz::make_span(rest_locals);
  ltm_job.output = ozz::make_span(rest_models);
  if (!ltm_job.Run()) {
    printf("DEBUG REST POSE: LocalToModelJob failed\n");
    return;
  }

  int n = std::min(count, num_joints);
  printf("DEBUG REST POSE: First %d joint positions (bind pose):\n", n);
  for (int i = 0; i < n; ++i) {
    const ozz::math::Float4x4& m = rest_models[i];
    float x = ozz::math::GetX(m.cols[3]);
    float y = ozz::math::GetY(m.cols[3]);
    float z = ozz::math::GetZ(m.cols[3]);
    const char* name = skel->joint_names()[i];
    printf("  [%d] '%s': (%.4f, %.4f, %.4f)\n", i, name, x, y, z);
  }
}
} // namespace eanim
