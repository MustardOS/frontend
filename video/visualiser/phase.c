#include "phase.h"

void wasabi_phase_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const int radius = (frame->width < frame->height ? frame->width : frame->height) * 3 / 8;
    SDL_Point points[WASABI_VISUALISER_SAMPLES];
    for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++) {
        const float left = frame->left[index];
        const float right =
            frame->channels > 1 ? frame->right[index] : frame->mono[(index + 13) % WASABI_VISUALISER_SAMPLES];
        points[index].x = frame->width / 2 + (int) (left * radius);
        points[index].y = frame->height / 2 - (int) (right * radius);
    }
    SDL_SetRenderDrawColor(renderer, frame->primary.r, frame->primary.g, frame->primary.b, 170);
    SDL_RenderDrawLine(
        renderer, frame->width / 2 - radius, frame->height / 2, frame->width / 2 + radius, frame->height / 2
    );
    SDL_RenderDrawLine(
        renderer, frame->width / 2, frame->height / 2 - radius, frame->width / 2, frame->height / 2 + radius
    );
    SDL_SetRenderDrawColor(renderer, frame->secondary.r, frame->secondary.g, frame->secondary.b, frame->secondary.a);
    SDL_RenderDrawLines(renderer, points, WASABI_VISUALISER_SAMPLES);
}
