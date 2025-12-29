// gl_wrappers.h - GL function wrappers for jank interop on Linux
//
// On Linux with GLEW, OpenGL functions are function pointers loaded at runtime,
// not regular functions. Jank's cpp/ interop expects regular callable functions.
// These inline wrappers provide that interface uniformly across platforms.
//
// Include this header in any .jank file that needs to call GL functions directly
// via cpp/ interop (after including GL headers).

#ifndef GL_WRAPPERS_H
#define GL_WRAPPERS_H

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
// On macOS, VAO functions need extern C declaration
extern "C" {
  void glGenVertexArrays(GLsizei n, GLuint *arrays);
  void glBindVertexArray(GLuint array);
  void glGenerateMipmap(GLenum target);
}
#else
#include <GL/glew.h>
#endif

// Buffer operations
inline void wrap_glGenBuffers(GLsizei n, GLuint *buffers) { glGenBuffers(n, buffers); }
inline void wrap_glBindBuffer(GLenum target, GLuint buffer) { glBindBuffer(target, buffer); }
inline void wrap_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { glBufferData(target, size, data, usage); }
inline void wrap_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { glBufferSubData(target, offset, size, data); }

// Vertex array operations
inline void wrap_glGenVertexArrays(GLsizei n, GLuint *arrays) { glGenVertexArrays(n, arrays); }
inline void wrap_glBindVertexArray(GLuint array) { glBindVertexArray(array); }
inline void wrap_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) { glVertexAttribPointer(index, size, type, normalized, stride, pointer); }
inline void wrap_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) { glVertexAttribIPointer(index, size, type, stride, pointer); }
inline void wrap_glEnableVertexAttribArray(GLuint index) { glEnableVertexAttribArray(index); }

// Draw operations
inline void wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) { glDrawArrays(mode, first, count); }
inline void wrap_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) { glDrawElements(mode, count, type, indices); }

// Shader operations
inline GLuint wrap_glCreateShader(GLenum type) { return glCreateShader(type); }
inline void wrap_glShaderSource(GLuint shader, GLsizei count, char **string, const GLint *length) { glShaderSource(shader, count, (const GLchar **)string, length); }
inline void wrap_glCompileShader(GLuint shader) { glCompileShader(shader); }
inline void wrap_glGetShaderiv(GLuint shader, GLenum pname, GLint *params) { glGetShaderiv(shader, pname, params); }
inline void wrap_glDeleteShader(GLuint shader) { glDeleteShader(shader); }

// Program operations
inline GLuint wrap_glCreateProgram() { return glCreateProgram(); }
inline void wrap_glAttachShader(GLuint program, GLuint shader) { glAttachShader(program, shader); }
inline void wrap_glLinkProgram(GLuint program) { glLinkProgram(program); }
inline void wrap_glGetProgramiv(GLuint program, GLenum pname, GLint *params) { glGetProgramiv(program, pname, params); }
inline void wrap_glUseProgram(GLuint program) { glUseProgram(program); }

// Uniform operations
inline GLint wrap_glGetUniformLocation(GLuint program, const GLchar *name) { return glGetUniformLocation(program, name); }
inline void wrap_glUniform1i(GLint location, GLint v0) { glUniform1i(location, v0); }
inline void wrap_glUniform1f(GLint location, GLfloat v0) { glUniform1f(location, v0); }
inline void wrap_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { glUniform3f(location, v0, v1, v2); }
inline void wrap_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { glUniform4f(location, v0, v1, v2, v3); }
inline void wrap_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) { glUniformMatrix4fv(location, count, transpose, value); }

// State operations
inline void wrap_glEnable(GLenum cap) { glEnable(cap); }
inline void wrap_glDisable(GLenum cap) { glDisable(cap); }
inline void wrap_glClear(GLbitfield mask) { glClear(mask); }
inline void wrap_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) { glClearColor(red, green, blue, alpha); }
inline void wrap_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { glViewport(x, y, width, height); }
inline void wrap_glBlendFunc(GLenum sfactor, GLenum dfactor) { glBlendFunc(sfactor, dfactor); }

// Texture operations
inline void wrap_glGenTextures(GLsizei n, GLuint *textures) { glGenTextures(n, textures); }
inline void wrap_glBindTexture(GLenum target, GLuint texture) { glBindTexture(target, texture); }
inline void wrap_glActiveTexture(GLenum texture) { glActiveTexture(texture); }
inline void wrap_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) { glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
inline void wrap_glTexParameteri(GLenum target, GLenum pname, GLint param) { glTexParameteri(target, pname, param); }
inline void wrap_glGenerateMipmap(GLenum target) { glGenerateMipmap(target); }

#endif // GL_WRAPPERS_H
