#pragma once

#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

namespace edemo {
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

inline glm::mat4* new_identity_matrix () {
    return new glm::mat4(1.0f);
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
    // Upload identity matrices for debugging
    std::vector<float> identity_bones(count * 16);
    for (int i = 0; i < count; ++i) {
      // Identity matrix
      identity_bones[i*16 + 0] = 1.0f;
      identity_bones[i*16 + 5] = 1.0f;
      identity_bones[i*16 + 10] = 1.0f;
      identity_bones[i*16 + 15] = 1.0f;
    }
    glUniformMatrix4fv(loc, count, GL_FALSE, identity_bones.data());
}

} // namespace edemo
