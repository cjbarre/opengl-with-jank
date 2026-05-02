#pragma once
#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstddef>

#include <jank/aot/resource.hpp>

namespace eshaders {
inline GLuint compile_shader_from_source(const char* source_data, std::size_t source_len,
                                         GLenum shader_type, const char* label) {
  GLuint shader = glCreateShader(shader_type);
  GLint len_int = (GLint)source_len;
  const char* sources[1] = { source_data };
  glShaderSource(shader, 1, sources, &len_int);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    fprintf(stderr, "Shader compilation failed: %s\n%s\n", label, info_log);
    return 0;
  }

  return shader;
}

inline GLuint compile_shader_from_file(const char* path, GLenum shader_type) {
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

  GLuint shader = compile_shader_from_source(source, (std::size_t)size, shader_type, path);
  free(source);
  return shader;
}

inline GLuint compile_shader_from_resource(const char* resource_name, GLenum shader_type) {
  auto opt = jank::aot::find_resource(jtl::immutable_string{ resource_name });
  if (!opt.is_some()) {
    fprintf(stderr, "Shader resource not found: %s\n", resource_name);
    return 0;
  }
  auto view = opt.unwrap();
  return compile_shader_from_source(view.data(), view.size(), shader_type, resource_name);
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
