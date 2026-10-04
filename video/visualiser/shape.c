#include "shape.h"

#include <math.h>

#define SHAPE_TAU      6.28318530717958647692f
#define SHAPE_SEGMENTS 64

static SDL_Vertex shape_vertex(const float x, const float y, const SDL_Color colour) {
    SDL_Vertex vertex;
    vertex.position.x = x;
    vertex.position.y = y;
    vertex.color = colour;
    vertex.tex_coord.x = 0.0f;
    vertex.tex_coord.y = 0.0f;
    return vertex;
}

SDL_Color vis_alpha(const SDL_Color colour, const int alpha) {
    const int clamped = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    return (SDL_Color) {colour.r, colour.g, colour.b, (Uint8) clamped};
}

void vis_sector(
    SDL_Renderer *renderer, const float x, const float y, const float inner, const float outer, const float start,
    const float end, const SDL_Color colour
) {
    if (outer <= 0.0f || end <= start) return;
    int segments = (int) ((end - start) / SHAPE_TAU * SHAPE_SEGMENTS) + 1;
    if (segments > SHAPE_SEGMENTS) segments = SHAPE_SEGMENTS;
    SDL_Vertex vertices[(SHAPE_SEGMENTS + 1) * 2];
    int indices[SHAPE_SEGMENTS * 6];
    for (int point = 0; point <= segments; point++) {
        const float angle = start + (end - start) * (float) point / (float) segments;
        const float cosine = cosf(angle);
        const float sine = sinf(angle);
        vertices[point * 2] = shape_vertex(x + cosine * inner, y + sine * inner, colour);
        vertices[point * 2 + 1] = shape_vertex(x + cosine * outer, y + sine * outer, colour);
    }
    for (int segment = 0; segment < segments; segment++) {
        int *index = &indices[segment * 6];
        const int base = segment * 2;
        index[0] = base;
        index[1] = base + 1;
        index[2] = base + 2;
        index[3] = base + 1;
        index[4] = base + 3;
        index[5] = base + 2;
    }
    SDL_RenderGeometry(renderer, NULL, vertices, (segments + 1) * 2, indices, segments * 6);
}

void vis_ring(
    SDL_Renderer *renderer, const float x, const float y, const float inner, const float outer, const SDL_Color colour
) {
    vis_sector(renderer, x, y, inner, outer, 0.0f, SHAPE_TAU, colour);
}

void vis_fill_circle(SDL_Renderer *renderer, const float x, const float y, const float radius, const SDL_Color colour) {
    if (radius <= 0.0f) return;
    SDL_Vertex vertices[SHAPE_SEGMENTS + 1];
    int indices[SHAPE_SEGMENTS * 3];
    vertices[0] = shape_vertex(x, y, colour);
    for (int point = 0; point < SHAPE_SEGMENTS; point++) {
        const float angle = SHAPE_TAU * (float) point / SHAPE_SEGMENTS;
        vertices[point + 1] = shape_vertex(x + cosf(angle) * radius, y + sinf(angle) * radius, colour);
        indices[point * 3] = 0;
        indices[point * 3 + 1] = point + 1;
        indices[point * 3 + 2] = point + 1 < SHAPE_SEGMENTS ? point + 2 : 1;
    }
    SDL_RenderGeometry(renderer, NULL, vertices, SHAPE_SEGMENTS + 1, indices, SHAPE_SEGMENTS * 3);
}

void vis_quad(
    SDL_Renderer *renderer, const float x0, const float y0, const float x1, const float y1, const float x2,
    const float y2, const float x3, const float y3, const SDL_Color colour
) {
    const SDL_Vertex vertices[4] = {
        shape_vertex(x0, y0, colour), shape_vertex(x1, y1, colour), shape_vertex(x2, y2, colour),
        shape_vertex(x3, y3, colour)
    };
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
}

void vis_line(
    SDL_Renderer *renderer, const float x0, const float y0, const float x1, const float y1, const float width,
    const SDL_Color colour
) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = sqrtf(dx * dx + dy * dy);
    if (length <= 0.0f) return;
    const float half = (width < 1.0f ? 1.0f : width) * 0.5f;
    const float nx = -dy / length * half;
    const float ny = dx / length * half;
    vis_quad(renderer, x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, colour);
}

void vis_round_rect(
    SDL_Renderer *renderer, const float x, const float y, const float width, const float height, float radius,
    const SDL_Color colour
) {
    if (width <= 0.0f || height <= 0.0f) return;
    if (radius * 2.0f > width) radius = width * 0.5f;
    if (radius * 2.0f > height) radius = height * 0.5f;
    vis_quad(
        renderer, x + radius, y, x + width - radius, y, x + width - radius, y + height, x + radius, y + height, colour
    );
    vis_quad(
        renderer, x, y + radius, x + radius, y + radius, x + radius, y + height - radius, x, y + height - radius, colour
    );
    vis_quad(
        renderer, x + width - radius, y + radius, x + width, y + radius, x + width, y + height - radius,
        x + width - radius, y + height - radius, colour
    );
    if (radius <= 0.0f) return;
    const float quarter = SHAPE_TAU * 0.25f;
    vis_sector(renderer, x + radius, y + radius, 0.0f, radius, quarter * 2.0f, quarter * 3.0f, colour);
    vis_sector(renderer, x + width - radius, y + radius, 0.0f, radius, quarter * 3.0f, quarter * 4.0f, colour);
    vis_sector(renderer, x + width - radius, y + height - radius, 0.0f, radius, 0.0f, quarter, colour);
    vis_sector(renderer, x + radius, y + height - radius, 0.0f, radius, quarter, quarter * 2.0f, colour);
}
