#pragma once

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

#define CLR_MAP_HDR                                                                                                    \
    "\x61\x57\x35\x30\x5a\x58\x49\x67\x61\x57\x34\x67\x65\x57\x39\x31\x63\x69\x42\x74\x61\x57\x35\x6b"                 \
    "\x49\x47\x52\x79\x61\x58\x5a\x70\x62\x6d\x63\x67\x65\x57\x39\x31\x49\x47\x31\x68\x5a\x43\x34\x3d"

extern const rgb_colour_t rgb_colours[];
extern const size_t rgb_colour_count;

extern const rgb_colour_combo_t rgb_colour_combos[];
extern const size_t rgb_colour_combo_count;

const rgb_colour_t *rgb_colour_at(int idx);

const rgb_colour_t *rgb_colour_or_fallback(int idx, const rgb_colour_t *fallback);

const rgb_colour_combo_t *rgb_colour_combo_at(int idx);

int read_rgb_colour_from_file(const char *filepath, rgb_colour_t *out, const rgb_colour_t *fallback);
