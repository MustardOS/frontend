#include "waveform.h"

static void draw_channel(
    SDL_Renderer *renderer, const float *samples, const int centre, const int amplitude,
    const wasabi_visualiser_frame *frame, const SDL_Color colour
) {
    SDL_Point points[WASABI_VISUALISER_SAMPLES];
    for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++) {
        float value = samples[index];
        if (value < -1.0f) value = -1.0f;
        if (value > 1.0f) value = 1.0f;
        points[index].x = index * (frame->width - 1) / (WASABI_VISUALISER_SAMPLES - 1);
        points[index].y = centre - (int) (value * amplitude);
    }
    SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
    SDL_RenderDrawLines(renderer, points, WASABI_VISUALISER_SAMPLES);
}

void wasabi_waveform_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    if (frame->channels > 1) {
        draw_channel(renderer, frame->left, frame->height * 2 / 5, frame->height / 7, frame, frame->primary);
        draw_channel(renderer, frame->right, frame->height * 3 / 5, frame->height / 7, frame, frame->secondary);
    } else {
        draw_channel(renderer, frame->mono, frame->height / 2, frame->height / 4, frame, frame->primary);
    }
}
