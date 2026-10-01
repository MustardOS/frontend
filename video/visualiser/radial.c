#include "radial.h"

#include <math.h>

#define RADIAL_POINTS 128
#define VIS_TAU 6.28318530717958647692f

static float phase;

void wasabi_radial_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const int maximum = frame->width < frame->height ? frame->width : frame->height;
    const float base = maximum * (0.18f + frame->level * 0.06f);
    const float range = maximum * 0.18f;
    SDL_Point points[RADIAL_POINTS + 1];
    phase += 0.006f + frame->level * 0.012f;
    for (int index = 0; index <= RADIAL_POINTS; index++) {
        const int sample = (index % RADIAL_POINTS) * WASABI_VISUALISER_SAMPLES / RADIAL_POINTS;
        const float amplitude = fabsf(frame->mono[sample]);
        const float radius = base + amplitude * range;
        const float angle = VIS_TAU * index / RADIAL_POINTS + phase;
        points[index].x = frame->width / 2 + (int) (cosf(angle) * radius);
        points[index].y = frame->height / 2 + (int) (sinf(angle) * radius);
    }
    SDL_SetRenderDrawColor(renderer, frame->primary.r, frame->primary.g, frame->primary.b, frame->primary.a);
    SDL_RenderDrawLines(renderer, points, RADIAL_POINTS + 1);
}

void wasabi_radial_reset(void) {
    phase = 0.0f;
}
