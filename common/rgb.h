#pragma once

typedef enum {
    RGB_MODE_OFF = 0,
    RGB_MODE_STATIC = 1,
    RGB_MODE_BREATHING = 2,
    RGB_MODE_PRESET_COMBO = 3,
    RGB_MODE_THEME_SUPPLIED = 4,
    RGB_MODE_COLOUR_CYCLE = 7,
    RGB_MODE_RAINBOW = 8,
    RGB_MODE_STICK_FOLLOW = 9,
} rgb_mode_t;

typedef enum {
    RGB_BREATH_FAST = 0,
    RGB_BREATH_MEDIUM,
    RGB_BREATH_SLOW,
} rgb_breath_speed_t;

typedef enum {
    RGB_BACKEND_AUTO = 0,
    RGB_BACKEND_SYSFS,
    RGB_BACKEND_SERIAL,
    RGB_BACKEND_JOYPAD,
} rgb_backend_t;
