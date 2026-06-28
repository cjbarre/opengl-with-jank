#pragma once

#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include <string>
#include <cstdlib>

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
