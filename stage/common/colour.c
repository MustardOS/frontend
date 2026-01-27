#include <math.h>
#include <string.h>
#include "../../common/log.h"
#include "common.h"
#include "colour.h"

#define COLOUR_OVERLAY OVERLAY_RUNNER "col_"

static struct {
    struct colour_state value;
    int loaded;
} colour_cache;

static int read_colour_value(const char *key, float *out) {
    char path[PATH_MAX];

    snprintf(path, sizeof(path), COLOUR_OVERLAY "%s", key);
    if (read_float(path, out)) return 1;

    snprintf(path, sizeof(path), COLOUR_CONFIG "%s", key);
    if (read_float(path, out)) return 1;

    return 0;
}

const struct colour_state *colour_adjust_get(void) {
    if (colour_cache.loaded) return &colour_cache.value;

    struct colour_state colour = {
            .brightness = 0.0f,
            .contrast   = 1.0f,
            .saturation = 1.0f,
            .hue        = 0.0f,
            .gamma      = 1.0f
    };

    float raw;

    if (read_colour_value("brightness", &raw)) {
        colour.brightness = clamp_float(raw / 100.0f, -1.0f, 1.0f);
        LOG_SUCCESS("stage", "Colour Brightness: %.3f", colour.brightness);
    }

    if (read_colour_value("contrast", &raw)) {
        colour.contrast = clamp_float(raw / 100.0f, 0.0f, 2.0f);
        LOG_SUCCESS("stage", "Colour Contrast: %.3f", colour.contrast);
    }

    if (read_colour_value("saturation", &raw)) {
        colour.saturation = clamp_float(raw / 100.0f, 0.0f, 2.0f);
        LOG_SUCCESS("stage", "Colour Saturation: %.3f", colour.saturation);
    }

    if (read_colour_value("hue", &raw)) {
        colour.hue = clamp_float(raw, -180.0f, 180.0f) * (float) M_PI / 180.0f;
        LOG_SUCCESS("stage", "Colour Hue: %.3f rad", colour.hue);
    }

    if (read_colour_value("gamma", &raw)) {
        colour.gamma = clamp_float(raw / 100.0f, 0.1f, 4.0f);
        LOG_SUCCESS("stage", "Colour Gamma: %.3f", colour.gamma); // who?
    }

    colour_cache.value = colour;
    colour_cache.loaded = 1;

    return &colour_cache.value;
}

void colour_adjust_reset(void) {
    colour_cache.loaded = 0;
}
