#pragma once

#define TWEAK_SLOTS 16
#define TWEAK_ITEMS 8

enum style_tweak_field {
    tweak_dx,
    tweak_dy,
    tweak_dw,
    tweak_dh,
    tweak_angle,
    tweak_tilt_x,
    tweak_tilt_y,
    tweak_depth,
    tweak_opacity,
    tweak_glyph,
    TWEAK_FIELD_COUNT
};

enum style_cascade_field {
    cascade_size,
    cascade_spacing,
    cascade_angle,
    cascade_tilt,
    cascade_fade,
    CASCADE_FIELD_COUNT
};

enum style_motion { motion_default, motion_glide, motion_swing, motion_orbit, MOTION_COUNT };

#define BOUNCE_HEIGHT 8
#define BOUNCE_SPEED  420

enum style_number {
    number_size,
    number_spacing,
    number_tilt,
    number_swell,
    number_text,
    number_focus,
    number_gap,
    number_slant,
    number_textbg,
    number_textx,
    number_texty,
    number_textangle,
    number_namex,
    number_namey,
    number_nameangle,
    number_namebg,
    number_mascotscale,
    number_glyphscale,
    number_namewidth,
    number_radius,
    NUMBER_COUNT
};

extern const char *const style_number_keys[NUMBER_COUNT];

enum style_tweak_target { tweak_target_override, tweak_target_package, tweak_target_global, TWEAK_TARGET_COUNT };

typedef struct {
    unsigned mask;
    int value[TWEAK_FIELD_COUNT];
} style_tweak;

typedef struct {
    int cascade[CASCADE_FIELD_COUNT];
    int motion;
    int time;
    int swell;
    int number[NUMBER_COUNT];
    unsigned number_set;
    int bounce_height;
    int bounce_speed;
    int bounce_set;
    int mascot_x;
    int mascot_y;
    char options[32];
    style_tweak slots[TWEAK_SLOTS];
    style_tweak items[TWEAK_ITEMS];
} style_tweaks;

extern const char *const style_tweak_keys[TWEAK_FIELD_COUNT];

extern const char *const style_cascade_keys[CASCADE_FIELD_COUNT];

style_tweaks *style_tweaks_current(void);

void style_tweaks_clear(void);

void style_tweaks_load(int style);

int style_tweaks_remove_all(void);

int style_tweaks_saved(int style);

void style_tweaks_remove(int style);

int style_tweaks_source(void);

const style_tweaks *style_tweaks_package(void);

int style_tweaks_save(int style, int target);

int style_tweak_get(const style_tweak *tweak, int field, int fallback);

void style_tweak_set(style_tweak *tweak, int field, int value);

void style_tweak_unset(style_tweak *tweak, int field);
