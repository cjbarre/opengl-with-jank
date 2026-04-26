#pragma once

#include "gl_wrappers.h"
#include <GLFW/glfw3.h>
#include "animation_types.h"
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstring>

namespace pskel {
// GLEW initialization for Linux
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

inline GLuint compile_shader_inline(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

inline GLuint create_line_shader(const char* vs, const char* fs) {
    GLuint vert = compile_shader_inline(GL_VERTEX_SHADER, vs);
    GLuint frag = compile_shader_inline(GL_FRAGMENT_SHADER, fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
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

inline bool starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

inline void print_bone_lines(float* vertices, int line_count, int max_print) {
    std::cout << std::endl << "=== BONE LINES BEING RENDERED ===" << std::endl;
    for (int i = 0; i < line_count && i < max_print; ++i) {
        int offset = i * 6;
        std::cout << "  Line " << i << ": ("
                  << vertices[offset + 0] << ", "
                  << vertices[offset + 1] << ", "
                  << vertices[offset + 2] << ") -> ("
                  << vertices[offset + 3] << ", "
                  << vertices[offset + 4] << ", "
                  << vertices[offset + 5] << ")" << std::endl;
    }
    std::cout << "=== END BONE LINES (showing "
              << (line_count < max_print ? line_count : max_print)
              << " of " << line_count << ") ===" << std::endl;
}

// Debug: Compare animation model matrices vs rest pose model matrices
inline void debug_compare_anim_vs_rest(AnimationContext* ctx) {
    if (!ctx || !ctx->skeleton) return;

    ozz::animation::Skeleton* skel = ctx->skeleton;
    int num_joints = skel->num_joints();
    int num_soa = skel->num_soa_joints();

    // Compute REST POSE model matrices
    std::vector<ozz::math::SoaTransform> rest_locals(num_soa);
    std::vector<ozz::math::Float4x4> rest_models(num_joints);
    for (int i = 0; i < num_soa; ++i) {
      rest_locals[i] = skel->joint_rest_poses()[i];
    }
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = skel;
    ltm_job.input = ozz::make_span(rest_locals);
    ltm_job.output = ozz::make_span(rest_models);
    ltm_job.Run();

    // Compare with animation model matrices (already in ctx->models)
    printf("\n=== COMPARING ANIMATION vs REST POSE ===\n");
    printf("%-20s  %-24s  %-24s  %s\n", "Joint", "REST (x,y,z)", "ANIM (x,y,z)", "Delta");

    float max_delta = 0;
    int max_delta_joint = -1;

    for (int i = 0; i < std::min(15, num_joints); ++i) {
      // Rest position
      float rx = ozz::math::GetX(rest_models[i].cols[3]);
      float ry = ozz::math::GetY(rest_models[i].cols[3]);
      float rz = ozz::math::GetZ(rest_models[i].cols[3]);

      // Animation position
      float ax = ozz::math::GetX(ctx->models[i].cols[3]);
      float ay = ozz::math::GetY(ctx->models[i].cols[3]);
      float az = ozz::math::GetZ(ctx->models[i].cols[3]);

      float delta = sqrtf((rx-ax)*(rx-ax) + (ry-ay)*(ry-ay) + (rz-az)*(rz-az));
      if (delta > max_delta) {
        max_delta = delta;
        max_delta_joint = i;
      }

      printf("%-20s  (%.3f, %.3f, %.3f)  (%.3f, %.3f, %.3f)  %.3f\n",
             skel->joint_names()[i], rx, ry, rz, ax, ay, az, delta);
    }

    printf("\nMax delta: %.3f at joint %d ('%s')\n",
           max_delta, max_delta_joint,
           max_delta_joint >= 0 ? skel->joint_names()[max_delta_joint] : "none");

    // Debug: Print ARM joint positions specifically (search by name)
    printf("\n=== ARM JOINT POSITIONS (humerus bones) ===\n");
    for (int i = 0; i < num_joints; ++i) {
      const char* name = skel->joint_names()[i];
      if (strstr(name, "humerus") != nullptr && strstr(name, "X") == nullptr) {
        float rx = ozz::math::GetX(rest_models[i].cols[3]);
        float ry = ozz::math::GetY(rest_models[i].cols[3]);
        float rz = ozz::math::GetZ(rest_models[i].cols[3]);
        float ax = ozz::math::GetX(ctx->models[i].cols[3]);
        float ay = ozz::math::GetY(ctx->models[i].cols[3]);
        float az = ozz::math::GetZ(ctx->models[i].cols[3]);
        int parent_idx = skel->joint_parents()[i];
        const char* parent_name = parent_idx >= 0 ? skel->joint_names()[parent_idx] : "none";
        printf("  Joint[%d] '%s' (parent=%d '%s'):\n", i, name, parent_idx, parent_name);
        printf("    REST:  (%.4f, %.4f, %.4f)\n", rx, ry, rz);
        printf("    ANIM:  (%.4f, %.4f, %.4f)\n", ax, ay, az);
      }
    }

    // Also print thoracic (parent of both arms)
    for (int i = 0; i < num_joints; ++i) {
      if (strcmp(skel->joint_names()[i], "thoracic") == 0) {
        float rx = ozz::math::GetX(rest_models[i].cols[3]);
        float ry = ozz::math::GetY(rest_models[i].cols[3]);
        float rz = ozz::math::GetZ(rest_models[i].cols[3]);
        float ax = ozz::math::GetX(ctx->models[i].cols[3]);
        float ay = ozz::math::GetY(ctx->models[i].cols[3]);
        float az = ozz::math::GetZ(ctx->models[i].cols[3]);
        printf("  Joint[%d] 'thoracic':\n", i);
        printf("    REST:  (%.4f, %.4f, %.4f)\n", rx, ry, rz);
        printf("    ANIM:  (%.4f, %.4f, %.4f)\n", ax, ay, az);
        break;
      }
    }
    printf("=== END ARM JOINTS ===\n");

    // Also print the LOCAL transforms from animation (what SamplingJob produced)
    printf("\n=== ANIMATION LOCAL TRANSFORMS (from SamplingJob) ===\n");
    printf("%-20s  %-30s  %-30s\n", "Joint", "Local Translation", "Local Rotation (quat)");

    // Access SoaTransform data - need to transpose for individual joints
    for (int soa_idx = 0; soa_idx < std::min(4, num_soa); ++soa_idx) {
      const ozz::math::SoaTransform& soa = ctx->locals[soa_idx];

      // ozz stores 4 joints per SoaTransform using SIMD
      // We need to extract individual components from the SIMD vectors
      alignas(16) float tx[4], ty[4], tz[4];
      alignas(16) float rx[4], ry[4], rz[4], rw[4];

      // Store SIMD vectors to arrays (ozz cross-platform)
      ozz::math::StorePtrU(soa.translation.x, tx);
      ozz::math::StorePtrU(soa.translation.y, ty);
      ozz::math::StorePtrU(soa.translation.z, tz);
      ozz::math::StorePtrU(soa.rotation.x, rx);
      ozz::math::StorePtrU(soa.rotation.y, ry);
      ozz::math::StorePtrU(soa.rotation.z, rz);
      ozz::math::StorePtrU(soa.rotation.w, rw);

      for (int lane = 0; lane < 4; ++lane) {
        int joint = soa_idx * 4 + lane;
        if (joint >= num_joints) break;

        printf("%-20s  (%.4f, %.4f, %.4f)        (%.4f, %.4f, %.4f, %.4f)\n",
               skel->joint_names()[joint],
               tx[lane], ty[lane], tz[lane],
               rx[lane], ry[lane], rz[lane], rw[lane]);
      }
    }

    // Also compare REST POSE local transforms
    printf("\n=== REST POSE LOCAL TRANSFORMS ===\n");
    printf("%-20s  %-30s  %-30s\n", "Joint", "Local Translation", "Local Rotation (quat)");
    for (int soa_idx = 0; soa_idx < std::min(4, num_soa); ++soa_idx) {
      const ozz::math::SoaTransform& soa = skel->joint_rest_poses()[soa_idx];

      alignas(16) float tx[4], ty[4], tz[4];
      alignas(16) float rx[4], ry[4], rz[4], rw[4];

      ozz::math::StorePtrU(soa.translation.x, tx);
      ozz::math::StorePtrU(soa.translation.y, ty);
      ozz::math::StorePtrU(soa.translation.z, tz);
      ozz::math::StorePtrU(soa.rotation.x, rx);
      ozz::math::StorePtrU(soa.rotation.y, ry);
      ozz::math::StorePtrU(soa.rotation.z, rz);
      ozz::math::StorePtrU(soa.rotation.w, rw);

      for (int lane = 0; lane < 4; ++lane) {
        int joint = soa_idx * 4 + lane;
        if (joint >= num_joints) break;

        printf("%-20s  (%.4f, %.4f, %.4f)        (%.4f, %.4f, %.4f, %.4f)\n",
               skel->joint_names()[joint],
               tx[lane], ty[lane], tz[lane],
               rx[lane], ry[lane], rz[lane], rw[lane]);
      }
    }

    printf("=== END DEBUG ===\n\n");
}

} // namespace pskel
