#include "orbit.h"

#include <math.h>

#define VIS_TAU 6.28318530717958647692f

static float phase;

void wasabi_orbit_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    phase += 0.018f + frame->level * 0.05f;
    const int maximum = frame->width < frame->height ? frame->width : frame->height;
    for (int orbit = 0; orbit < 4; orbit++) {
        const SDL_Color colour = orbit & 1 ? frame->secondary : frame->primary;
        const float radius = maximum * (0.12f + orbit * 0.07f) * (0.8f + frame->level * 0.35f);
        SDL_Point points[97];
        for (int index = 0; index <= 96; index++) {
            const float angle = VIS_TAU * index / 96.0f;
            const float wobble = 1.0f + frame->mono[index * (WASABI_VISUALISER_SAMPLES - 1) / 96] * 0.18f;
            points[index].x = frame->width / 2 + (int) (cosf(angle + phase * (orbit + 1)) * radius * wobble);
            points[index].y = frame->height / 2 + (int) (sinf(angle - phase * 0.5f) * radius * wobble);
        }
        SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
        SDL_RenderDrawLines(renderer, points, 97);
    }
}

void wasabi_orbit_reset(void) {
    phase = 0.0f;
}
