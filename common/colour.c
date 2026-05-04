#include "colour.h"
#include "common.h"

const rgb_colour_t RGB_COLOURS[] = {
        {"Off",           0,   0,   0},

        // Reds
        {"Red",           255, 0,   0},
        {"Scarlet",       255, 36,  0},

        // Oranges
        {"Peach",         255, 157, 129},
        {"Salmon",        171, 82,  54},
        {"Light Orange",  255, 87,  34},
        {"Coral",         255, 127, 80},
        {"Pastel Orange", 210, 105, 30},
        {"Orange",        255, 130, 0},
        {"Tangerine",     242, 133, 0},

        // Ambers
        {"Amber",         255, 191, 0},
        {"Mustard",       225, 173, 1},
        {"Gold",          255, 215, 0},
        {"Vanilla",       255, 236, 39},
        {"Lemon",         255, 247, 0},
        {"Olive",         128, 128, 0},
        {"Yellow",        255, 255, 0},

        // Greens
        {"Lime Green",    168, 231, 46},
        {"Forest Green",  34,  139, 34},
        {"Green",         0,   255, 0},
        {"Mint",          152, 255, 152},
        {"Pistachio",     50,  255, 50},
        {"Neon Green",    0,   228, 54},
        {"Emerald",       80,  200, 120},
        {"Seafoam",       159, 226, 191},

        // Cyans
        {"Light Blue",    64,  224, 208},
        {"Neon Cyan",     0,   255, 255},
        {"Teal",          0,   128, 128},
        {"Powder Blue",   176, 224, 230},
        {"Sky Blue",      135, 206, 235},

        // Blues
        {"Blue",          41,  173, 255},
        {"Cobalt",        0,   71,  171},
        {"Navy",          0,   0,   128},
        {"True Blue",     0,   0,   255},

        // Purples
        {"Neon Purple",   128, 0,   255},
        {"Violet",        143, 0,   255},
        {"Indigo",        75,  0,   130},
        {"Purple",        160, 32,  240},
        {"Lavender",      131, 70,  156},
        {"Lilac",         200, 162, 200},
        {"Magenta",       255, 0,   255},
        {"Plum",          142, 69,  133},

        // Pinks
        {"Bubblegum",     255, 105, 180},
        {"Pink",          190, 18,  80},
        {"Blossom",       255, 119, 168},
        {"Rose",          255, 102, 153},
        {"Crimson",       220, 20,  60},
        {"Hot Pink",      255, 20,  40},

        // Neutrals
        {"Silver",        192, 192, 192},
        {"Warm White",    255, 244, 229},
        {"Cool White",    240, 248, 255},
        {"White",         255, 255, 255},
};

const size_t RGB_COLOUR_COUNT = sizeof(RGB_COLOURS) / sizeof(RGB_COLOURS[0]);

const rgb_colour_combo_t RGB_COLOUR_COMBOS[] = {
        {"Switch",       255, 0,   0,   0,   0,   255},
        {"Melon",        255, 0,   0,   0,   255, 0},
        {"Neon",         255, 0,   30,  0,   228, 54},
        {"Tropical",     255, 69,  0,   0,   255, 0},
        {"Cotton Candy", 255, 236, 39,  255, 119, 168},
        {"Sunset",       255, 94,  19,  138, 43,  226},
        {"Aurora",       0,   255, 127, 64,  64,  255},
        {"Cyberpunk",    0,   255, 255, 255, 0,   180},
        {"Lava",         255, 30,  0,   255, 200, 0},
        {"Ocean",        0,   105, 148, 64,  224, 208},
        {"Forest",       34,  139, 34,  101, 67,  33},
        {"Galaxy",       72,  0,   255, 255, 0,   200},
        {"Strawberry",   220, 20,  60,  255, 182, 193},
        {"Vapourwave",   255, 113, 206, 1,   205, 254},
};

const size_t RGB_COLOUR_COMBO_COUNT = sizeof(RGB_COLOUR_COMBOS) / sizeof(RGB_COLOUR_COMBOS[0]);

static int hex_to_byte(const char *hex, uint8_t *out) {
    char buf[3] = { hex[0], hex[1], '\0' };

    char *endptr;
    long val = strtol(buf, &endptr, 16);

    if (*endptr != '\0' || val < 0 || val > 255) {
        return 0;
    }

    *out = (uint8_t)val;
    return 1;
}

const rgb_colour_t *rgb_colour_at(int idx) {
    if (idx < 0 || idx >= (int) RGB_COLOUR_COUNT) idx = 0;

    return &RGB_COLOURS[idx];
}

const rgb_colour_t *rgb_colour_or_fallback(int idx, const rgb_colour_t *fallback) {
    if (idx <= 0) return fallback ? fallback : &RGB_COLOURS[0];

    return rgb_colour_at(idx - 1);
}

int read_rgb_colour_from_file(const char *filepath, rgb_colour_t *out, const rgb_colour_t *fallback) {
    if (!file_exist(filepath)) {
        *out = *fallback;
        return 0;
    }

    const char *line = read_line_char_from(filepath, 1);
    if (!line || *line == '\0') {
        *out = *fallback;
        return 0;
    }

    // --- Try parsing as integer index ---
    char *endptr;
    long idx = strtol(line, &endptr, 10);

    if (*endptr == '\0') {
        // Entire string was a valid integer

        size_t count = sizeof(RGB_COLOURS) / sizeof(RGB_COLOURS[0]);

        if (idx >= 0 && (size_t)idx < count) {
            *out = RGB_COLOURS[idx];
            return 1;
        } else {
            *out = *fallback;
            return 0;
        }
    }

    // --- Otherwise treat as hex ---
    const char *hex = line;

    if (hex[0] == '#') {
        hex++;
    }

    if (strlen(hex) != 6) {
        *out = *fallback;
        return 0;
    }

    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            *out = *fallback;
            return 0;
        }
    }

    uint8_t r, g, b;

    if (!hex_to_byte(hex, &r) ||
        !hex_to_byte(hex + 2, &g) ||
        !hex_to_byte(hex + 4, &b)) {
        *out = *fallback;
        return 0;
    }

    out->name = NULL;
    out->r = r;
    out->g = g;
    out->b = b;

    return 1;
}

const rgb_colour_combo_t *rgb_colour_combo_at(int idx) {
    if (idx < 0 || idx >= (int) RGB_COLOUR_COMBO_COUNT) idx = 0;

    return &RGB_COLOUR_COMBOS[idx];
}
