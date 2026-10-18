#include "orbit.h"

#include <math.h>

#define VIS_TAU      6.28318530717958647692f
#define ORBIT_POINTS 96

static float phase;
static float unit_x[ORBIT_POINTS + 1];
static float unit_y[ORBIT_POINTS + 1];
static int prepared;

void wasabi_orbit_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    phase += 0.018f + frame->level * 0.05f;
    if (phase >= VIS_TAU) phase -= VIS_TAU;
    const int maximum = frame->width < frame->height ? frame->width : frame->height;
    const float half_phase = phase * 0.5f;
    const float half_cos = cosf(half_phase);
    const float half_sin = sinf(half_phase);
    for (int orbit = 0; orbit < 4; orbit++) {
        const SDL_Color colour = orbit & 1 ? frame->secondary : frame->primary;
        const float radius = maximum * (0.12f + orbit * 0.07f) * (0.8f + frame->level * 0.35f);
        SDL_Point points[ORBIT_POINTS + 1];
        const float orbit_phase = phase * (float) (orbit + 1);
        const float orbit_cos = cosf(orbit_phase);
        const float orbit_sin = sinf(orbit_phase);
        for (int index = 0; index <= ORBIT_POINTS; index++) {
            const float wobble = 1.0f + frame->mono[index * (WASABI_VISUALISER_SAMPLES - 1) / ORBIT_POINTS] * 0.18f;
            const float x = unit_x[index] * orbit_cos - unit_y[index] * orbit_sin;
            const float y = unit_y[index] * half_cos - unit_x[index] * half_sin;
            points[index].x = frame->width / 2 + (int) (x * radius * wobble);
            points[index].y = frame->height / 2 + (int) (y * radius * wobble);
        }
        SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
        SDL_RenderDrawLines(renderer, points, ORBIT_POINTS + 1);
    }
}

void wasabi_orbit_reset(void) {
    if (!prepared) {
        for (int index = 0; index <= ORBIT_POINTS; index++) {
            const float angle = VIS_TAU * (float) index / ORBIT_POINTS;
            unit_x[index] = cosf(angle);
            unit_y[index] = sinf(angle);
        }
        prepared = 1;
    }
    phase = 0.0f;
}
