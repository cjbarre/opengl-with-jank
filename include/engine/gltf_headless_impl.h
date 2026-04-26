#pragma once
#include "cgltf.h"
#include <cstring>

namespace egltf_hl {
inline bool node_is_colonly(cgltf_node* node) {
  if (!node || !node->name) return false;
  const char* str = node->name;
  const char* suffix = "-colonly";
  size_t len = strlen(str);
  size_t slen = 8;
  if (len < slen) return false;
  return strcmp(str + len - slen, suffix) == 0;
}

inline cgltf_float* get_node_translation (cgltf_node* node) {
  return node->translation;
}

inline cgltf_float* get_node_scale (cgltf_node* node) {
  return node->scale;
}
} // namespace egltf_hl
