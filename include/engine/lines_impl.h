#pragma once
#include "gl_wrappers.h"

namespace elines {
// Create line VAO and VBO, returns them via output parameters
inline void create_line_vao(int max_lines, unsigned int* vao_out, unsigned int* vbo_out) {
  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, max_lines * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
  *vao_out = vao;
  *vbo_out = vbo;
}

// Update line VBO with vertex data
inline void update_line_vbo(unsigned int vbo, float* vertices, int line_count) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, line_count * 6 * sizeof(float), vertices);
}

} // namespace elines
