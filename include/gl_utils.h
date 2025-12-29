#ifndef GL_UTILS_H
#define GL_UTILS_H

#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Common utility for vertex attribute pointers
inline void* voidify_int(int i) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(i));
}

// Float size helper
inline size_t float_size_fn() { return sizeof(float); }
#define float_size (float_size_fn())

// Identity matrix creation
inline glm::mat4 identity_matrix() {
    return glm::mat4(1.0f);
}

#endif // GL_UTILS_H
