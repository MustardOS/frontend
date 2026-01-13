#pragma once

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>
#include "../common/common.h"

#define BASE_OVERLAY_NOP "/run/muos/overlay.disable"

#define BASE_ALPHA OVERLAY_CONFIG "gen_alpha"
#define BASE_ANCHOR OVERLAY_CONFIG "gen_anchor"
#define BASE_SCALE OVERLAY_CONFIG "gen_scale"

extern int base_anchor_cached;
extern int base_scale_cached;

extern SDL_Texture *base_sdl_tex;
extern int base_sdl_ready;
extern int base_sdl_w;
extern int base_sdl_h;

extern GLuint base_gles_tex;
extern int base_gles_w;
extern int base_gles_h;
extern int base_gles_attempted;
extern int base_gles_ready;

extern gl_vtx_t vtx_base[4];
extern int vtx_base_valid;

extern int sdl_overlay_resolved;
extern char sdl_overlay_path_last[PATH_MAX];
extern uint64_t sdl_overlay_last_check_ms;

extern int base_overlay_disabled_cached;
extern int base_nop_last;

// The following structs are used, but attributes can't be used here apparently!
extern struct flag_cache base_enable_cache;
extern struct alpha_cache base_alpha_cache;
extern struct anchor_cache base_anchor_cache;
extern struct scale_cache base_scale_cache;

extern const struct overlay_resolver GLES_RESOLVER;
extern const struct overlay_resolver SDL_RESOLVER;

void base_inotify_check(void);

void destroy_base_gles(void);

void sdl_base_overlay_init(SDL_Renderer *renderer);

void gl_base_overlay_init(SDL_Window *window);
