#include "visualiser.h"

#include <math.h>
#include <string.h>

#include <module/muxshare.h>

#include "meter.h"
#include "orbit.h"
#include "phase.h"
#include "pulse.h"
#include "radial.h"
#include "spectrum.h"
#include "starfield.h"
#include "waveform.h"

#define VISUALISER_PERIOD 33
#define VISUALISER_CAPTURE_PERIOD 16

static SDL_SpinLock sample_lock;
static float captured_mono[WASABI_VISUALISER_SAMPLES];
static float captured_left[WASABI_VISUALISER_SAMPLES];
static float captured_right[WASABI_VISUALISER_SAMPLES];
static int captured_channels;
static uint32_t next_frame;
static uint32_t next_capture;

static SDL_Color visualiser_colour(const uint32_t colour, const uint8_t alpha) {
    return (SDL_Color) {
        (uint8_t) (colour >> 16), (uint8_t) (colour >> 8), (uint8_t) colour, alpha
    };
}

void wasabi_visualiser_capture(const float *samples, const int frames, const int channels) {
    if (!samples || frames <= 0 || channels <= 0 || config.video.visualiser <= wasabi_visualiser_disabled) return;
    const uint32_t now = SDL_GetTicks();
    if (next_capture && !SDL_TICKS_PASSED(now, next_capture)) return;
    next_capture = now + VISUALISER_CAPTURE_PERIOD;

    float mono[WASABI_VISUALISER_SAMPLES];
    float left[WASABI_VISUALISER_SAMPLES];
    float right[WASABI_VISUALISER_SAMPLES];
    const int stride = frames > WASABI_VISUALISER_SAMPLES ? frames / WASABI_VISUALISER_SAMPLES : 1;
    int source = frames - stride * WASABI_VISUALISER_SAMPLES;
    if (source < 0) source = 0;

    for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++) {
        const int frame = source + index * stride;
        if (frame >= frames) {
            mono[index] = left[index] = right[index] = 0.0f;
            continue;
        }
        left[index] = samples[(size_t) frame * channels];
        right[index] = channels > 1 ? samples[(size_t) frame * channels + 1] : left[index];
        mono[index] = (left[index] + right[index]) * 0.5f;
    }

    SDL_AtomicLock(&sample_lock);
    memcpy(captured_mono, mono, sizeof(captured_mono));
    memcpy(captured_left, left, sizeof(captured_left));
    memcpy(captured_right, right, sizeof(captured_right));
    captured_channels = channels;
    SDL_AtomicUnlock(&sample_lock);
}

void wasabi_visualiser_render(SDL_Renderer *renderer) {
    if (!renderer || config.video.visualiser <= wasabi_visualiser_disabled
        || config.video.visualiser >= wasabi_visualiser_count)
        return;

    wasabi_visualiser_frame frame = {0};
    if (SDL_GetRendererOutputSize(renderer, &frame.width, &frame.height) != 0
        || frame.width <= 0 || frame.height <= 0)
        return;

    SDL_AtomicLock(&sample_lock);
    memcpy(frame.mono, captured_mono, sizeof(frame.mono));
    memcpy(frame.left, captured_left, sizeof(frame.left));
    memcpy(frame.right, captured_right, sizeof(frame.right));
    frame.channels = captured_channels;
    SDL_AtomicUnlock(&sample_lock);

    float total = 0.0f;
    for (int index = 0; index < WASABI_VISUALISER_SAMPLES; index++) total += frame.mono[index] * frame.mono[index];
    frame.level = sqrtf(total / WASABI_VISUALISER_SAMPLES) * 4.0f;
    if (frame.level > 1.0f) frame.level = 1.0f;
    frame.ticks = SDL_GetTicks();
    frame.primary = visualiser_colour(theme.list_focus.background, 220);
    frame.secondary = visualiser_colour(theme.list_default.text, 190);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    switch ((wasabi_visualiser_mode) config.video.visualiser) {
        case wasabi_visualiser_spectrum: wasabi_spectrum_render(renderer, &frame); break;
        case wasabi_visualiser_waveform: wasabi_waveform_render(renderer, &frame); break;
        case wasabi_visualiser_pulse: wasabi_pulse_render(renderer, &frame); break;
        case wasabi_visualiser_orbit: wasabi_orbit_render(renderer, &frame); break;
        case wasabi_visualiser_meter: wasabi_meter_render(renderer, &frame); break;
        case wasabi_visualiser_phase: wasabi_phase_render(renderer, &frame); break;
        case wasabi_visualiser_radial: wasabi_radial_render(renderer, &frame); break;
        case wasabi_visualiser_starfield: wasabi_starfield_render(renderer, &frame); break;
        default: break;
    }
}

int wasabi_visualiser_tick(void) {
    if (config.video.visualiser <= wasabi_visualiser_disabled) return 0;
    const uint32_t now = SDL_GetTicks();
    if (!SDL_TICKS_PASSED(now, next_frame)) return 0;
    next_frame = now + VISUALISER_PERIOD;
    return 1;
}

void wasabi_visualiser_reset(void) {
    SDL_AtomicLock(&sample_lock);
    memset(captured_mono, 0, sizeof(captured_mono));
    memset(captured_left, 0, sizeof(captured_left));
    memset(captured_right, 0, sizeof(captured_right));
    captured_channels = 0;
    SDL_AtomicUnlock(&sample_lock);
    wasabi_spectrum_reset();
    wasabi_orbit_reset();
    wasabi_meter_reset();
    wasabi_radial_reset();
    wasabi_starfield_reset();
    next_frame = 0;
    next_capture = 0;
}
