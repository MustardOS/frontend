#pragma once

#include <SDL2/SDL.h>

#define WASABI_VISUALISER_SAMPLES 256

typedef enum {
    wasabi_visualiser_disabled = 0,
    wasabi_visualiser_spectrum,
    wasabi_visualiser_waveform,
    wasabi_visualiser_pulse,
    wasabi_visualiser_orbit,
    wasabi_visualiser_meter,
    wasabi_visualiser_phase,
    wasabi_visualiser_radial,
    wasabi_visualiser_starfield,
    wasabi_visualiser_count
} wasabi_visualiser_mode;

typedef struct {
    float mono[WASABI_VISUALISER_SAMPLES];
    float left[WASABI_VISUALISER_SAMPLES];
    float right[WASABI_VISUALISER_SAMPLES];
    float level;
    int channels;
    int width;
    int height;
    uint32_t ticks;
    SDL_Color primary;
    SDL_Color secondary;
} wasabi_visualiser_frame;

void wasabi_visualiser_capture(const float *samples, int frames, int channels);
void wasabi_visualiser_render(SDL_Renderer *renderer);
int wasabi_visualiser_tick(void);
void wasabi_visualiser_reset(void);
