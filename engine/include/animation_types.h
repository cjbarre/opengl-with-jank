#ifndef ANIMATION_TYPES_H
#define ANIMATION_TYPES_H

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/containers/vector.h"
#include "ozz_mesh.h"

// Animation context holds all runtime buffers needed for animation
struct AnimationContext {
  ozz::animation::Skeleton* skeleton;
  ozz::vector<ozz::animation::Animation*> animations;

  // Runtime buffers
  ozz::vector<ozz::math::SoaTransform> locals;  // Local space transforms
  ozz::vector<ozz::math::Float4x4> models;       // Model space matrices
  ozz::animation::SamplingJob::Context context;  // Sampling context

  AnimationContext() : skeleton(nullptr) {}

  ~AnimationContext() {
    delete skeleton;
    for (auto* anim : animations) {
      delete anim;
    }
  }

  bool init(int num_soa_joints, int num_joints) {
    locals.resize(num_soa_joints);
    models.resize(num_joints);
    context.Resize(num_joints);
    return true;
  }
};

// Compute and upload skinning matrices directly to shader
inline void compute_and_upload_skinning_matrices(
  AnimationContext* ctx,
  ozz::vector<ozz::sample::Mesh>* meshes,
  int mesh_index,
  GLint uniform_location
) {
  if (!ctx || !meshes || mesh_index < 0 || mesh_index >= static_cast<int>(meshes->size())) {
    return;
  }
  const auto& mesh = (*meshes)[mesh_index];
  size_t joint_count = mesh.joint_remaps.size();

  // Allocate temp buffer
  std::vector<float> matrices(joint_count * 16);

  for (size_t i = 0; i < joint_count; ++i) {
    ozz::math::Float4x4 skinning_matrix =
        ctx->models[mesh.joint_remaps[i]] * mesh.inverse_bind_poses[i];
    // Float4x4 has 4 SimdFloat4 columns - need to store each column properly
    float* out = matrices.data() + i * 16;
    ozz::math::StorePtrU(skinning_matrix.cols[0], out + 0);
    ozz::math::StorePtrU(skinning_matrix.cols[1], out + 4);
    ozz::math::StorePtrU(skinning_matrix.cols[2], out + 8);
    ozz::math::StorePtrU(skinning_matrix.cols[3], out + 12);
  }

  glUniformMatrix4fv(uniform_location, static_cast<GLsizei>(joint_count), GL_FALSE, matrices.data());
}

#endif // ANIMATION_TYPES_H
