#pragma once
#include "gl_wrappers.h"
#include "ozz/base/containers/vector.h"
#include "ozz_mesh.h"
#include <vector>
#include <cstring>
#include <cstdint>

// Skinned vertex layout matching skinned_vertex.glsl
// Total stride: 48 bytes
struct SkinnedVertex {
  float pos[3];       // 12 bytes, offset 0
  float norm[3];      // 12 bytes, offset 12
  float uv[2];        // 8 bytes,  offset 24
  uint16_t joints[4]; // 8 bytes,  offset 32
  float weights[4];   // 16 bytes, offset 40
};

namespace emesh {
// Build interleaved vertex buffer from ozz mesh parts
// Returns vertex count, fills output buffer
inline int build_skinned_vertices(
  ozz::vector<ozz::sample::Mesh>* meshes,
  int mesh_index,
  std::vector<SkinnedVertex>& out_vertices
) {
  if (!meshes || mesh_index < 0 || mesh_index >= static_cast<int>(meshes->size())) {
    return 0;
  }
  const auto& mesh = (*meshes)[mesh_index];

  // Count total vertices across all parts
  int total_verts = mesh.vertex_count();
  out_vertices.resize(total_verts);

  int vertex_offset = 0;
  for (size_t pi = 0; pi < mesh.parts.size(); ++pi) {
    const auto& part = mesh.parts[pi];
    int part_verts = part.vertex_count();
    int influences = part.influences_count();

    for (int vi = 0; vi < part_verts; ++vi) {
      SkinnedVertex& v = out_vertices[vertex_offset + vi];

      // Position (3 floats per vertex)
      v.pos[0] = part.positions[vi * 3 + 0];
      v.pos[1] = part.positions[vi * 3 + 1];
      v.pos[2] = part.positions[vi * 3 + 2];

      // Normal (3 floats per vertex)
      if (!part.normals.empty()) {
        v.norm[0] = part.normals[vi * 3 + 0];
        v.norm[1] = part.normals[vi * 3 + 1];
        v.norm[2] = part.normals[vi * 3 + 2];
      } else {
        v.norm[0] = 0.0f; v.norm[1] = 1.0f; v.norm[2] = 0.0f;
      }

      // UV (2 floats per vertex)
      if (!part.uvs.empty()) {
        v.uv[0] = part.uvs[vi * 2 + 0];
        v.uv[1] = part.uvs[vi * 2 + 1];
      } else {
        v.uv[0] = 0.0f; v.uv[1] = 0.0f;
      }

      // Joint indices (influences per vertex, pad to 4)
      for (int i = 0; i < 4; ++i) {
        if (i < influences && !part.joint_indices.empty()) {
          v.joints[i] = part.joint_indices[vi * influences + i];
        } else {
          v.joints[i] = 0;
        }
      }

      // Joint weights (influences-1 per vertex because last is implicit, pad to 4)
      // ozz stores N-1 weights, last weight = 1 - sum(others)
      float weight_sum = 0.0f;
      for (int i = 0; i < 4; ++i) {
        if (i < influences - 1 && !part.joint_weights.empty()) {
          v.weights[i] = part.joint_weights[vi * (influences - 1) + i];
          weight_sum += v.weights[i];
        } else if (i == influences - 1 && influences > 0) {
          // Last weight is implicit
          v.weights[i] = 1.0f - weight_sum;
        } else {
          v.weights[i] = 0.0f;
        }
      }
    }
    vertex_offset += part_verts;
  }
  return total_verts;
}

inline void* int_to_ptr(size_t offset) {
  return reinterpret_cast<void*>(offset);
}

inline size_t skinned_vertex_size() {
  return sizeof(SkinnedVertex);
}

// Wrapper for glVertexAttribIPointer (integer attributes)
inline void set_vertex_attrib_i_pointer(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset) {
  glVertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(offset));
}
} // namespace emesh
