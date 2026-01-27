#include <math.h>
#include <string.h>
#include "../../common/log.h"
#include "common.h"
#include "colour.h"

static struct {
    struct colour_state value;
    int loaded;
} colour_cache;

const struct colour_state *colour_get(void) {
    if (colour_cache.loaded) return &colour_cache.value;

    struct colour_state colour = {
            .brightness = 0.0f,
            .contrast   = 1.0f,
            .saturation = 1.0f,
            .hue        = 0.0f,
            .gamma      = 1.0f
    };

    float raw;

    if (read_float(COLOUR_CONFIG "brightness", &raw)) {
        colour.brightness = clamp_float(raw / 100.0f, -1.0f, 1.0f);
        LOG_SUCCESS("stage", "Colour Brightness: %.3f", colour.brightness);
    }

    if (read_float(COLOUR_CONFIG "contrast", &raw)) {
        colour.contrast = clamp_float(raw / 100.0f, 0.0f, 2.0f);
        LOG_SUCCESS("stage", "Colour Contrast: %.3f", colour.contrast);
    }

    if (read_float(COLOUR_CONFIG "saturation", &raw)) {
        colour.saturation = clamp_float(raw / 100.0f, 0.0f, 2.0f);
        LOG_SUCCESS("stage", "Colour Saturation: %.3f", colour.saturation);
    }

    if (read_float(COLOUR_CONFIG "hue", &raw)) {
        colour.hue = clamp_float(raw, -180.0f, 180.0f) * (float) M_PI / 180.0f;
        LOG_SUCCESS("stage", "Colour Hue: %.3f rad", colour.hue);
    }

    if (read_float(COLOUR_CONFIG "gamma", &raw)) {
        colour.gamma = clamp_float(raw / 100.0f, 0.1f, 4.0f);
        LOG_SUCCESS("stage", "Colour Gamma: %.3f", colour.gamma); // who?
    }

    colour_cache.value = colour;
    colour_cache.loaded = 1;

    return &colour_cache.value;
}

void colour_reset(void) {
    colour_cache.loaded = 0;
}
