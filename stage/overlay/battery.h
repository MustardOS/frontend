#pragma once

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>
#include "../common/common.h"

#define BATTERY_DETECT OVERLAY_RUNNER "battery"

#define BATTERY_ALPHA OVERLAY_CONFIG "bat_alpha"
#define BATTERY_ANCHOR OVERLAY_CONFIG "bat_anchor"
#define BATTERY_SCALE OVERLAY_CONFIG "bat_scale"

#define BATTERY_OFFSET INTERNAL_CONFIG "settings/advanced/offset"

struct offset_cache {
    const char *path;
    time_t mtime;
    int value;
};

extern char battery_overlay_path[PATH_MAX];

extern int battery_last_step;

extern int battery_anchor_cached;
extern int battery_scale_cached;
extern int battery_rotate_cached;

extern SDL_Texture *battery_sdl_tex[INDICATOR_STEPS];
extern int battery_sdl_w[INDICATOR_STEPS];
extern int battery_sdl_h[INDICATOR_STEPS];
extern int battery_preload_sdl_done;
extern int battery_disabled_sdl;

extern GLuint battery_gles_tex[INDICATOR_STEPS];
extern int battery_gles_w[INDICATOR_STEPS];
extern int battery_gles_h[INDICATOR_STEPS];
extern int battery_preload_gles_done;
extern int battery_disabled_gles;

extern gl_vtx_t vtx_battery[4];
extern int vtx_battery_valid;

extern struct offset_cache battery_offset_cache;
extern struct flag_cache battery_enable_cache;
extern struct alpha_cache battery_alpha_cache;
extern struct anchor_cache battery_anchor_cache;
extern struct scale_cache battery_scale_cache;

int battery_overlay_enabled(void);

void battery_overlay_update(void);

void sdl_battery_overlay_init(SDL_Renderer *renderer);

void gl_battery_overlay_init(void);
