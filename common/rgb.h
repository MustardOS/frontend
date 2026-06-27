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
    rgb_breath_fast = 0,
    rgb_breath_medium,
    rgb_breath_slow,
} rgb_breath_speed_t;

typedef enum {
    rgb_backend_auto = 0,
    rgb_backend_sysfs,
    rgb_backend_serial,
    rgb_backend_joypad,
} rgb_backend_t;
