#pragma once
#include "cgltf.h"
#include <cstring>

struct Vertex {
  float pos[3];
  float norm[3];
  float uv[2];

  Vertex(float px, float py, float pz,
         float nx, float ny, float nz,
         float u, float v) {
      pos[0]=px; pos[1]=py; pos[2]=pz;
      norm[0]=nx; norm[1]=ny; norm[2]=nz;
      uv[0]=u; uv[1]=v;
  }

  Vertex() { // default ctor for array allocation
      pos[0]=pos[1]=pos[2]=0.0f;
      norm[0]=norm[1]=norm[2]=0.0f;
      uv[0]=uv[1]=0.0f;
  }
};

inline size_t int_size_fn() { return sizeof(int); }
#define int_size (int_size_fn())

inline size_t vertex_size_fn() { return sizeof(Vertex); }
#define vertex_size (vertex_size_fn())

namespace egltf {
inline bool cgltf_primitive_has_material(cgltf_primitive* prim) {
    return (prim->material != nullptr);
}

inline cgltf_float* get_node_translation (cgltf_node* node) {
  return node->translation;
}

inline cgltf_float* get_node_scale (cgltf_node* node) {
  return node->scale;
}

inline bool node_is_colonly(cgltf_node* node) {
  if (!node || !node->name) return false;
  const char* str = node->name;
  const char* suffix = "-colonly";
  size_t len = strlen(str);
  size_t slen = 8;
  if (len < slen) return false;
  return strcmp(str + len - slen, suffix) == 0;
}
} // namespace egltf
