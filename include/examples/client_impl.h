#pragma once

#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include <string>
#include <cstdlib>
#include <vector>

namespace eclient {
// GLEW initialization for Linux - must be called after glfwMakeContextCurrent
inline bool init_glew() {
#ifndef __APPLE__
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
        return false;
    }
    // Clear any GL errors from glewInit (common issue)
    while (glGetError() != GL_NO_ERROR) {}
    return true;
#else
    return true;
#endif
}

inline void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void) window;
    glViewport(0, 0, width, height);
}

inline GLuint create_line_vao(GLuint* vbo_out) {
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 128 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    *vbo_out = vbo;
    return vao;
}

inline void update_line_vbo(GLuint vbo, float* vertices, int line_count) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, line_count * 6 * sizeof(float), vertices);
}

inline void upload_identity_bones(GLint loc, int count) {
    std::vector<float> identity_bones(count * 16);
    for (int i = 0; i < count; ++i) {
      identity_bones[i*16 + 0] = 1.0f;
      identity_bones[i*16 + 5] = 1.0f;
      identity_bones[i*16 + 10] = 1.0f;
      identity_bones[i*16 + 15] = 1.0f;
    }
    glUniformMatrix4fv(loc, count, GL_FALSE, identity_bones.data());
}

inline std::string parse_host_helper(const char* addr) {
    std::string s(addr);
    if (s.rfind("tcp://", 0) == 0) s = s.substr(6);
    auto pos = s.rfind(':');
    if (pos != std::string::npos) {
      return s.substr(0, pos);
    }
    return s;
}

inline int parse_port_helper(const char* addr, int default_port) {
    std::string s(addr);
    if (s.rfind("tcp://", 0) == 0) s = s.substr(6);
    auto pos = s.rfind(':');
    if (pos != std::string::npos) {
      return std::atoi(s.substr(pos + 1).c_str());
    }
    return default_port;
}

} // namespace eclient
