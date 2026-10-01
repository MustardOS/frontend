#include "spectrum.h"

#include <math.h>
#include <string.h>

#define SPECTRUM_BANDS 24
#define VIS_TAU 6.28318530717958647692f

static float smoothed[SPECTRUM_BANDS];
static float window[WASABI_VISUALISER_SAMPLES];
static float coefficient[SPECTRUM_BANDS];
static int prepared;

void wasabi_spectrum_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    const int margin = frame->width / 18;
    const int floor = frame->height * 4 / 5;
    const int available = frame->width - margin * 2;
    const int gap = available / SPECTRUM_BANDS / 5 > 2 ? available / SPECTRUM_BANDS / 5 : 2;
    const int bar_width = available / SPECTRUM_BANDS - gap;

    for (int band = 0; band < SPECTRUM_BANDS; band++) {
        float previous = 0.0f;
        float before_previous = 0.0f;
        for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++) {
            const float current = frame->mono[index] * window[index]
                                  + coefficient[band] * previous - before_previous;
            before_previous = previous;
            previous = current;
        }
        float power = previous * previous + before_previous * before_previous
                      - coefficient[band] * previous * before_previous;
        if (power < 0.0f) power = 0.0f;
        float value = sqrtf(power) / 18.0f;
        if (value > 1.0f) value = 1.0f;
        smoothed[band] = smoothed[band] * 0.72f + value * 0.28f;
        const int bar_height = (int) (smoothed[band] * frame->height * 0.55f) + 2;
        const SDL_Rect rectangle = {
            margin + band * available / SPECTRUM_BANDS, floor - bar_height, bar_width, bar_height
        };
        SDL_SetRenderDrawColor(renderer, frame->primary.r, frame->primary.g, frame->primary.b, frame->primary.a);
        SDL_RenderFillRect(renderer, &rectangle);
    }
}

void wasabi_spectrum_reset(void) {
    if (!prepared) {
        for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++)
            window[index] = 0.5f - 0.5f * cosf(VIS_TAU * index / (WASABI_VISUALISER_SAMPLES - 1));
        for (int band = 0; band < SPECTRUM_BANDS; band++)
            coefficient[band] = 2.0f * cosf(VIS_TAU * (band + 1) / WASABI_VISUALISER_SAMPLES);
        prepared = 1;
    }
    memset(smoothed, 0, sizeof(smoothed));
}
