#pragma once

#include "gl_wrappers.h"
#include <GLFW/glfw3.h>

namespace esd {

inline bool init_glew() {
#ifndef __APPLE__
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
        return false;
    }
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

} // namespace esd
