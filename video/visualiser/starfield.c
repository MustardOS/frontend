#include "starfield.h"

#define STAR_COUNT 72

typedef struct {
    float x;
    float y;
    float z;
} visualiser_star;

static visualiser_star stars[STAR_COUNT];
static uint32_t random_state = 0x5a17c9e3U;

static float random_unit(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (float) (random_state & 0xffffU) / 65535.0f;
}

static void reset_star(visualiser_star *star, const int distant) {
    star->x = random_unit() * 2.0f - 1.0f;
    star->y = random_unit() * 2.0f - 1.0f;
    star->z = distant ? 0.85f + random_unit() * 0.15f : 0.08f + random_unit() * 0.92f;
}

void wasabi_starfield_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const float speed = 0.006f + frame->level * 0.026f;
    const float scale = (float) (frame->width < frame->height ? frame->width : frame->height) * 0.42f;
    for (int index = 0; index < STAR_COUNT; index++) {
        visualiser_star *star = &stars[index];
        star->z -= speed;
        if (star->z <= 0.04f) reset_star(star, 1);
        const float perspective = 1.0f / star->z;
        const int x = frame->width / 2 + (int) (star->x * perspective * scale);
        const int y = frame->height / 2 + (int) (star->y * perspective * scale);
        if (x < 0 || x >= frame->width || y < 0 || y >= frame->height) {
            reset_star(star, 1);
            continue;
        }
        int size = (int) ((1.0f - star->z) * 3.0f) + 1;
        if (size > 4) size = 4;
        const SDL_Rect point = {x, y, size, size};
        const SDL_Color colour = index & 1 ? frame->secondary : frame->primary;
        SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
        SDL_RenderFillRect(renderer, &point);
    }
}

void wasabi_starfield_reset(void) {
    random_state = 0x5a17c9e3U;
    for (int index = 0; index < STAR_COUNT; index++) reset_star(&stars[index], 0);
}
