#pragma once
#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

namespace eshaders {
inline GLuint compile_shader_from_file(const char* path, GLenum shader_type) {
  // Read file
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open shader: %s\n", path);
    return 0;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);

  char* source = (char*)malloc(size + 1);
  if (!source) {
    fclose(f);
    return 0;
  }
  fread(source, 1, size, f);
  source[size] = '\0';
  fclose(f);

  // Compile shader
  GLuint shader = glCreateShader(shader_type);
  const char* sources[1] = { source };
  glShaderSource(shader, 1, sources, nullptr);
  glCompileShader(shader);

  free(source);

  // Check compilation
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    fprintf(stderr, "Shader compilation failed: %s\n%s\n", path, info_log);
    return 0;
  }

  return shader;
}

inline void log_compilation_error (unsigned int shader_id) {
  char info_log[512];
  glGetShaderInfoLog(shader_id, 512, NULL, info_log);
  printf("%s", info_log);
}

inline void log_link_error (unsigned int program_id) {
  char info_log[512];
  glGetProgramInfoLog(program_id, 512, NULL, info_log);
  printf("%s", info_log);
}
} // namespace eshaders
