#include "radial.h"

#include <math.h>

#define RADIAL_POINTS 128
#define VIS_TAU       6.28318530717958647692f

static float phase;
static float unit_x[RADIAL_POINTS + 1];
static float unit_y[RADIAL_POINTS + 1];
static int prepared;

void wasabi_radial_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const int maximum = frame->width < frame->height ? frame->width : frame->height;
    const float base = maximum * (0.18f + frame->level * 0.06f);
    const float range = maximum * 0.18f;
    SDL_Point points[RADIAL_POINTS + 1];
    phase += 0.006f + frame->level * 0.012f;
    if (phase >= VIS_TAU) phase -= VIS_TAU;
    const float phase_cos = cosf(phase);
    const float phase_sin = sinf(phase);
    for (int index = 0; index <= RADIAL_POINTS; index++) {
        const int sample = (index % RADIAL_POINTS) * WASABI_VISUALISER_SAMPLES / RADIAL_POINTS;
        const float amplitude = fabsf(frame->mono[sample]);
        const float radius = base + amplitude * range;
        const float x = unit_x[index] * phase_cos - unit_y[index] * phase_sin;
        const float y = unit_y[index] * phase_cos + unit_x[index] * phase_sin;
        points[index].x = frame->width / 2 + (int) (x * radius);
        points[index].y = frame->height / 2 + (int) (y * radius);
    }
    SDL_SetRenderDrawColor(renderer, frame->primary.r, frame->primary.g, frame->primary.b, frame->primary.a);
    SDL_RenderDrawLines(renderer, points, RADIAL_POINTS + 1);
}

void wasabi_radial_reset(void) {
    if (!prepared) {
        for (int index = 0; index <= RADIAL_POINTS; index++) {
            const float angle = VIS_TAU * (float) index / RADIAL_POINTS;
            unit_x[index] = cosf(angle);
            unit_y[index] = sinf(angle);
        }
        prepared = 1;
    }
    phase = 0.0f;
}
