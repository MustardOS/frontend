#include "colour.h"

const rgb_colour_t RGB_COLOURS[] = {
        {"Off",           0,   0,   0},
        {"Red",           255, 0,   0},
        {"Hot Pink",      255, 20,  40},
        {"Pink",          190, 18,  80},
        {"Blossom",       255, 119, 168},
        {"Peach",         255, 157, 129},
        {"Pastel Orange", 210, 105, 30},
        {"Salmon",        171, 82,  54},
        {"Light Orange",  255, 87,  34},
        {"Orange",        255, 130, 0},
        {"Mustard",       225, 173, 1},
        {"Yellow",        255, 255, 0},
        {"Olive",         128, 128, 0},
        {"Vanilla",       255, 236, 39},
        {"Lime Green",    168, 231, 46},
        {"Pistachio",     50,  255, 50},
        {"Green",         0,   255, 0},
        {"Neon Green",    0,   228, 54},
        {"Neon Cyan",     0,   255, 255},
        {"Light Blue",    64,  224, 208},
        {"Sky Blue",      135, 206, 235},
        {"Blue",          41,  173, 255},
        {"True Blue",     0,   0,   255},
        {"Neon Purple",   128, 0,   255},
        {"Purple",        160, 32,  240},
        {"Lavender",      131, 70,  156},
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

const rgb_colour_t *rgb_colour_at(int idx) {
    if (idx < 0 || idx >= (int) RGB_COLOUR_COUNT) idx = 0;

    return &RGB_COLOURS[idx];
}

const rgb_colour_t *rgb_colour_or_fallback(int idx, const rgb_colour_t *fallback) {
    if (idx <= 0) return fallback ? fallback : &RGB_COLOURS[0];

    return rgb_colour_at(idx - 1);
}

const rgb_colour_combo_t *rgb_colour_combo_at(int idx) {
    if (idx < 0 || idx >= (int) RGB_COLOUR_COMBO_COUNT) idx = 0;

    return &RGB_COLOUR_COMBOS[idx];
}
