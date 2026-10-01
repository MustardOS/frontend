#include "pulse.h"

#include <math.h>

#define VIS_TAU 6.28318530717958647692f

void wasabi_pulse_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const int maximum = frame->width < frame->height ? frame->width : frame->height;
    const int radius = maximum / 10 + (int) (frame->level * maximum / 4);
    const int centre_x = frame->width / 2;
    const int centre_y = frame->height / 2;

    for (int ring = 0; ring < 3; ring++) {
        const SDL_Color colour = ring & 1 ? frame->secondary : frame->primary;
        const int current = radius + ring * maximum / 18;
        SDL_Point points[65];
        for (int index = 0; index <= 64; index++) {
            const float angle = VIS_TAU * index / 64.0f;
            points[index].x = centre_x + (int) (cosf(angle) * current);
            points[index].y = centre_y + (int) (sinf(angle) * current);
        }
        SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
        SDL_RenderDrawLines(renderer, points, 65);
    }
}
