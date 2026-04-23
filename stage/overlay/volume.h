#pragma once

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>
#include "../common/common.h"

#define VOLUME_PATH GENERAL_CONFIG "volume"

#define VOLUME_ALPHA  OVERLAY_CONFIG "vol_alpha"
#define VOLUME_ANCHOR OVERLAY_CONFIG "vol_anchor"
#define VOLUME_SCALE  OVERLAY_CONFIG "vol_scale"

extern char volume_overlay_path[PATH_MAX];

extern struct stat volume_stat_last;
extern int volume_init;

extern int volume_visible;
extern int volume_last_step;
extern int volume_last_pct;
extern uint64_t volume_last_change_ms;

extern int volume_anchor_cached;
extern int volume_scale_cached;

extern SDL_Texture *volume_sdl_tex[INDICATOR_STEPS];
extern int volume_sdl_w[INDICATOR_STEPS];
extern int volume_sdl_h[INDICATOR_STEPS];

extern GLuint volume_gles_tex[INDICATOR_STEPS];
extern int volume_gles_w[INDICATOR_STEPS];
extern int volume_gles_h[INDICATOR_STEPS];

extern int volume_preload_sdl_done;
extern int volume_preload_gles_done;
extern int volume_disabled_sdl;
extern int volume_disabled_gles;

extern gl_vtx_t vtx_volume[4];
extern int vtx_volume_valid;

extern struct alpha_cache volume_alpha_cache;
extern struct anchor_cache volume_anchor_cache;
extern struct scale_cache volume_scale_cache;

void volume_overlay_init(void);

void volume_overlay_update(void);

int volume_is_visible(void);

void sdl_volume_overlay_init(SDL_Renderer *renderer);

void gl_volume_overlay_init(void);
