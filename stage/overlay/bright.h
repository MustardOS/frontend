#pragma once

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>
#include "../common/common.h"

#define BRIGHT_PATH GENERAL_CONFIG "brightness"

#define BRIGHT_ALPHA  OVERLAY_CONFIG "bri_alpha"
#define BRIGHT_ANCHOR OVERLAY_CONFIG "bri_anchor"
#define BRIGHT_SCALE  OVERLAY_CONFIG "bri_scale"

extern char bright_overlay_path[PATH_MAX];

extern struct stat bright_stat_last;
extern int bright_init;

extern int bright_visible;
extern int bright_last_step;
extern int bright_last_pct;
extern uint64_t bright_last_change_ms;

extern int bright_anchor_cached;
extern int bright_scale_cached;

extern SDL_Texture *bright_sdl_tex[INDICATOR_STEPS];
extern int bright_sdl_w[INDICATOR_STEPS];
extern int bright_sdl_h[INDICATOR_STEPS];

extern GLuint bright_gles_tex[INDICATOR_STEPS];
extern int bright_gles_w[INDICATOR_STEPS];
extern int bright_gles_h[INDICATOR_STEPS];

extern int bright_preload_sdl_done;
extern int bright_preload_gles_done;
extern int bright_disabled_sdl;
extern int bright_disabled_gles;

extern gl_vtx_t vtx_bright[4];
extern int vtx_bright_valid;

extern struct alpha_cache bright_alpha_cache;
extern struct anchor_cache bright_anchor_cache;
extern struct scale_cache bright_scale_cache;

void bright_overlay_init(void);

void bright_overlay_update(void);

int bright_is_visible(void);

void sdl_bright_overlay_init(SDL_Renderer *renderer);

void gl_bright_overlay_init(void);
