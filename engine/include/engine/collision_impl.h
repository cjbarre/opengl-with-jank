#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

namespace ecol {
// Raycast down from position, find highest ground below
// Returns ground Y coordinate, or -99999.0f if no ground found
inline float raycast_ground(
    std::vector<glm::vec3>* positions,
    std::vector<unsigned int>* indices,
    float px, float py, float pz
) {
    const float EPSILON = 0.000001f;
    float ray_height = py + 100.0f;
    glm::vec3 ray_origin(px, ray_height, pz);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);

    float closest_y = -99999.0f;
    bool found = false;

    size_t num_tris = indices->size() / 3;
    for (size_t i = 0; i < num_tris; i++) {
        glm::vec3 v0 = (*positions)[(*indices)[i*3]];
        glm::vec3 v1 = (*positions)[(*indices)[i*3+1]];
        glm::vec3 v2 = (*positions)[(*indices)[i*3+2]];

        // Moller-Trumbore ray-triangle intersection
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(ray_dir, edge2);
        float a = glm::dot(edge1, h);

        if (fabs(a) < EPSILON) continue;

        float f = 1.0f / a;
        glm::vec3 s = ray_origin - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray_dir, q);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * glm::dot(edge2, q);
        if (t > EPSILON) {
            float hit_y = ray_height - t;
            if (hit_y > closest_y) {
                closest_y = hit_y;
                found = true;
            }
        }
    }

    return found ? closest_y : -99999.0f;
}
// Ground raycast with surface normal output
// Returns ground Y, writes hit triangle normal to out_nx/ny/nz
// Returns -99999.0f if no ground found
inline float raycast_ground_normal(
    std::vector<glm::vec3>* positions,
    std::vector<unsigned int>* indices,
    float px, float py, float pz,
    float* out_nx, float* out_ny, float* out_nz
) {
    const float EPSILON = 0.000001f;
    float ray_height = py + 100.0f;
    glm::vec3 ray_origin(px, ray_height, pz);
    glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);

    float closest_y = -99999.0f;
    bool found = false;
    glm::vec3 hit_normal(0.0f, 1.0f, 0.0f);

    size_t num_tris = indices->size() / 3;
    for (size_t i = 0; i < num_tris; i++) {
        glm::vec3 v0 = (*positions)[(*indices)[i*3]];
        glm::vec3 v1 = (*positions)[(*indices)[i*3+1]];
        glm::vec3 v2 = (*positions)[(*indices)[i*3+2]];

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(ray_dir, edge2);
        float a = glm::dot(edge1, h);

        if (fabs(a) < EPSILON) continue;

        float f = 1.0f / a;
        glm::vec3 s = ray_origin - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray_dir, q);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * glm::dot(edge2, q);
        if (t > EPSILON) {
            float hit_y = ray_height - t;
            if (hit_y > closest_y) {
                closest_y = hit_y;
                found = true;
                // Compute triangle normal
                glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));
                // Ensure normal points upward
                if (n.y < 0) n = -n;
                hit_normal = n;
            }
        }
    }

    if (found) {
        *out_nx = hit_normal.x;
        *out_ny = hit_normal.y;
        *out_nz = hit_normal.z;
    }
    return found ? closest_y : -99999.0f;
}

// Horizontal raycast: fires along XZ plane
// Returns distance to nearest hit, or -1.0f if no hit within max_dist
inline float raycast_horizontal(
    std::vector<glm::vec3>* positions,
    std::vector<unsigned int>* indices,
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    float max_dist
) {
    const float EPSILON = 0.000001f;
    glm::vec3 ray_origin(ox, oy, oz);
    glm::vec3 ray_dir(dx, dy, dz);

    float closest_t = max_dist + 1.0f;
    bool found = false;

    size_t num_tris = indices->size() / 3;
    for (size_t i = 0; i < num_tris; i++) {
        glm::vec3 v0 = (*positions)[(*indices)[i*3]];
        glm::vec3 v1 = (*positions)[(*indices)[i*3+1]];
        glm::vec3 v2 = (*positions)[(*indices)[i*3+2]];

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(ray_dir, edge2);
        float a = glm::dot(edge1, h);

        if (fabs(a) < EPSILON) continue;

        float f = 1.0f / a;
        glm::vec3 s = ray_origin - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray_dir, q);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * glm::dot(edge2, q);
        if (t > EPSILON && t < closest_t) {
            closest_t = t;
            found = true;
        }
    }

    return found ? closest_t : -1.0f;
}
} // namespace ecol
