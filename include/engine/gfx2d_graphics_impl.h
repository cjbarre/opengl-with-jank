#pragma once

// Graphics2D state - stored in C++ to avoid closure capture issues
struct Graphics2DState {
    GLuint vao;
    GLuint vbo;
    GLuint shader;
};

inline Graphics2DState*& get_g_gfx2d() {
    static Graphics2DState* ptr = nullptr;
    return ptr;
}
#define g_gfx2d (get_g_gfx2d())

#define float_size_2d (egfx2d::get_float_size_2d())

namespace egfx2d {
inline void init_gfx2d_state(GLuint vao, GLuint vbo, GLuint shader) {
    if (g_gfx2d) delete g_gfx2d;
    g_gfx2d = new Graphics2DState();
    g_gfx2d->vao = vao;
    g_gfx2d->vbo = vbo;
    g_gfx2d->shader = shader;
}

inline size_t get_float_size_2d() { return sizeof(float); }

inline void* voidify_int_2d(int i) {
    return (void*)(intptr_t)i;
}

inline void begin_2d_impl(int screen_w, int screen_h) {
    if (!g_gfx2d) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(g_gfx2d->shader);

    glm::mat4 projection = glm::ortho(0.0f, (float)screen_w, (float)screen_h, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(g_gfx2d->shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(g_gfx2d->vao);
}

inline void end_2d_impl() {
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

inline void set_color_impl(float r, float g, float b, float a) {
    if (!g_gfx2d) return;
    glUniform4f(glGetUniformLocation(g_gfx2d->shader, "uColor"), r, g, b, a);
}

inline void render_line_impl(float x1, float y1, float x2, float y2, float thickness) {
    if (!g_gfx2d) return;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrt(dx*dx + dy*dy);
    if (len < 0.001f) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    float vertices[] = {
        x1 - nx, y1 - ny,
        x1 + nx, y1 + ny,
        x2 + nx, y2 + ny,

        x1 - nx, y1 - ny,
        x2 + nx, y2 + ny,
        x2 - nx, y2 - ny
    };

    glBindBuffer(GL_ARRAY_BUFFER, g_gfx2d->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

inline void render_arc_outline_impl(float cx, float cy, float radius, float start_angle, float end_angle, int segments, float thickness) {
    if (!g_gfx2d) return;

    std::vector<float> vertices;
    float angle_step = (end_angle - start_angle) / segments;
    float inner_r = radius - thickness * 0.5f;
    float outer_r = radius + thickness * 0.5f;

    for (int i = 0; i < segments; i++) {
        float a1 = start_angle + i * angle_step;
        float a2 = start_angle + (i + 1) * angle_step;

        float cos1 = cos(a1), sin1 = sin(a1);
        float cos2 = cos(a2), sin2 = sin(a2);

        vertices.push_back(cx + inner_r * cos1);
        vertices.push_back(cy + inner_r * sin1);
        vertices.push_back(cx + outer_r * cos1);
        vertices.push_back(cy + outer_r * sin1);
        vertices.push_back(cx + outer_r * cos2);
        vertices.push_back(cy + outer_r * sin2);

        vertices.push_back(cx + inner_r * cos1);
        vertices.push_back(cy + inner_r * sin1);
        vertices.push_back(cx + outer_r * cos2);
        vertices.push_back(cy + outer_r * sin2);
        vertices.push_back(cx + inner_r * cos2);
        vertices.push_back(cy + inner_r * sin2);
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_gfx2d->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);
}

inline void render_filled_arc_impl(float cx, float cy, float radius, float start_angle, float end_angle, int segments) {
    if (!g_gfx2d) return;

    std::vector<float> vertices;
    float angle_step = (end_angle - start_angle) / segments;

    for (int i = 0; i < segments; i++) {
        float a1 = start_angle + i * angle_step;
        float a2 = start_angle + (i + 1) * angle_step;

        vertices.push_back(cx);
        vertices.push_back(cy);
        vertices.push_back(cx + radius * cos(a1));
        vertices.push_back(cy + radius * sin(a1));
        vertices.push_back(cx + radius * cos(a2));
        vertices.push_back(cy + radius * sin(a2));
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_gfx2d->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);
}
} // namespace egfx2d
