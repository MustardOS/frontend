#include "meter.h"

#include <math.h>

static float level_left;
static float level_right;
static float peak_left;
static float peak_right;

static float channel_level(const float *samples) {
    float total = 0.0f;
    for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++)
        total += samples[index] * samples[index];
    const float value = sqrtf(total / WASABI_VISUALISER_SAMPLES) * 3.5f;
    return value > 1.0f ? 1.0f : value;
}

static void draw_meter(
    SDL_Renderer *renderer, const wasabi_visualiser_frame *frame, const int x, const float level, const float peak,
    const SDL_Color colour
) {
    const int segments = 24;
    const int width = frame->width / 12;
    const int height = frame->height * 3 / 5;
    const int gap = height / segments / 4 > 1 ? height / segments / 4 : 1;
    const int segment_height = height / segments - gap;
    const int base = frame->height * 4 / 5;
    const int active = (int) (level * segments + 0.5f);
    const int peak_segment = (int) (peak * (segments - 1));

    for (int segment = 0; segment < segments; segment++) {
        const SDL_Rect rectangle = {x, base - (segment + 1) * height / segments, width, segment_height};
        const uint8_t alpha = segment < active || segment == peak_segment ? colour.a : 42;
        SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, alpha);
        SDL_RenderFillRect(renderer, &rectangle);
    }
}

void wasabi_meter_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const float left = channel_level(frame->left);
    const float right = channel_level(frame->channels > 1 ? frame->right : frame->left);
    level_left = level_left * 0.68f + left * 0.32f;
    level_right = level_right * 0.68f + right * 0.32f;
    peak_left = left > peak_left ? left : peak_left * 0.975f;
    peak_right = right > peak_right ? right : peak_right * 0.975f;
    draw_meter(renderer, frame, frame->width * 3 / 8, level_left, peak_left, frame->primary);
    draw_meter(renderer, frame, frame->width * 13 / 24, level_right, peak_right, frame->secondary);
}

void wasabi_meter_reset(void) {
    level_left = level_right = peak_left = peak_right = 0.0f;
}
