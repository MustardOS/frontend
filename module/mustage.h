#pragma once

#include <time.h>
#include <stddef.h>

#define BATTERY_DETECT "/run/muos/overlay.battery"

#define OVERLAY_ANCHOR "/opt/muos/config/settings/overlay/gen_anchor"
#define BATTERY_ANCHOR "/opt/muos/config/settings/overlay/bat_anchor"

#define OVERLAY_ALPHA  "/opt/muos/config/settings/overlay/gen_alpha"
#define BATTERY_ALPHA  "/opt/muos/config/settings/overlay/bat_alpha"

#define OVERLAY_SCALE  "/opt/muos/config/settings/overlay/gen_scale"
#define BATTERY_SCALE  "/opt/muos/config/settings/overlay/bat_scale"

enum overlay_anchor {
    ANCHOR_TOP_LEFT = 0,
    ANCHOR_TOP_MIDDLE = 1,
    ANCHOR_TOP_RIGHT = 2,
    ANCHOR_CENTRE_LEFT = 3,
    ANCHOR_CENTRE_MIDDLE = 4,
    ANCHOR_CENTRE_RIGHT = 5,
    ANCHOR_BOTTOM_LEFT = 6,
    ANCHOR_BOTTOM_MIDDLE = 7,
    ANCHOR_BOTTOM_RIGHT = 8
};

enum overlay_scale {
    SCALE_ORIGINAL = 0,
    SCALE_FIT = 1,
    SCALE_STRETCH = 2
};

enum render_type {
    TYPE_SDL = 1,
    TYPE_GL = 2
};

struct anchor_cache {
    const char *path;
    time_t mtime;
    int value;
};

struct scale_cache {
    const char *path;
    time_t mtime;
    int value;
};

struct alpha_cache {
    const char *path;
    time_t mtime;
    float value;
};

struct flag_cache {
    const char *path;
    time_t mtime;
    int value;
};

struct overlay_go_cache {
    time_t mtime;
    char content[512];
    char system[256];
    char core[256];
    int valid;
};

struct overlay_resolver {
    int render_type;

    void (*get_dimension)(enum render_type type, void *ctx, char *out, size_t out_sz);
};

void get_dimension(enum render_type type, void *ctx, char *out, size_t out_sz);
