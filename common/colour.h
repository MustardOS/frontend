#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_colour_t;

typedef struct {
    const char *name;
    uint8_t a_r;
    uint8_t a_g;
    uint8_t a_b;
    uint8_t b_r;
    uint8_t b_g;
    uint8_t b_b;
} rgb_colour_combo_t;

extern const rgb_colour_t RGB_COLOURS[];
extern const size_t RGB_COLOUR_COUNT;

extern const rgb_colour_combo_t RGB_COLOUR_COMBOS[];
extern const size_t RGB_COLOUR_COMBO_COUNT;

const rgb_colour_t *rgb_colour_at(int idx);

const rgb_colour_t *rgb_colour_or_fallback(int idx, const rgb_colour_t *fallback);

const rgb_colour_combo_t *rgb_colour_combo_at(int idx);

int read_rgb_colour_from_file(const char *filepath, rgb_colour_t *out, const rgb_colour_t *fallback);
