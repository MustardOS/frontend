#pragma once

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>
#include "../common/common.h"

#define BATTERY_IMAGE  "battery"
#define BATTERY_DETECT "/run/muos/overlay.battery"

#define BATTERY_ALPHA OVERLAY_CONFIG "bat_alpha"
#define BATTERY_ANCHOR OVERLAY_CONFIG "bat_anchor"
#define BATTERY_SCALE OVERLAY_CONFIG "bat_scale"

extern char battery_overlay_path[PATH_MAX];
extern int battery_last_enabled;

extern int battery_anchor_cached;
extern int battery_scale_cached;

extern SDL_Texture *battery_sdl_tex;
extern int battery_sdl_ready;
extern int battery_sdl_attempted;
extern int battery_sdl_w;
extern int battery_sdl_h;

extern GLuint battery_gles_tex;
extern int battery_gles_ready;
extern int battery_gles_attempted;
extern int battery_gles_w;
extern int battery_gles_h;

extern gl_vtx_t vtx_battery[4];
extern int vtx_battery_valid;

extern struct flag_cache battery_enable_cache;
extern struct alpha_cache battery_alpha_cache;
extern struct anchor_cache battery_anchor_cache;
extern struct scale_cache battery_scale_cache;

int battery_overlay_enabled(void);

void battery_overlay_update(void);

void sdl_battery_overlay_init(SDL_Renderer *renderer);

void gl_battery_overlay_init(void);
