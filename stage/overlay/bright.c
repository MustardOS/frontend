#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "bright.h"

char bright_overlay_path[PATH_MAX];

struct stat bright_stat_last;
int bright_init = 0;
int bright_primed = 0;
int bright_visible = 0;
int bright_last_step = -1;
int bright_last_pct = -1;
int bright_device_max = 0;
uint64_t bright_last_change_ms = 0;

int bright_anchor_cached = -1;
int bright_scale_cached = -1;

SDL_Texture *bright_sdl_tex[INDICATOR_STEPS];
int bright_sdl_w[INDICATOR_STEPS];
int bright_sdl_h[INDICATOR_STEPS];

GLuint bright_gles_tex[INDICATOR_STEPS];
int bright_gles_w[INDICATOR_STEPS];
int bright_gles_h[INDICATOR_STEPS];

int bright_preload_sdl_done = 0;
int bright_preload_gles_done = 0;

int bright_disabled_sdl = 0;
int bright_disabled_gles = 0;

gl_vtx_t vtx_bright[4];
int vtx_bright_valid = 0;

struct alpha_cache bright_alpha_cache = {
        .path  = BRIGHT_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

struct anchor_cache bright_anchor_cache = {
        .path  = BRIGHT_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_BOTTOM_MIDDLE
};

struct scale_cache bright_scale_cache = {
        .path  = BRIGHT_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static int ensure_bright_path(enum render_method type, void *ctx, int step) {
    if (!ctx) return 0;

    switch (type) {
        case RENDER_SDL:
            get_dimension(RENDER_SDL, ctx, dimension, sizeof(dimension));
            break;
        case RENDER_GLES:
            get_dimension(RENDER_GLES, ctx, dimension, sizeof(dimension));
            break;
    }

    char name[64];
    snprintf(name, sizeof(name), "bright_%d", step);

    if (load_stage_image("bright", ovl_go_cache.core, ovl_go_cache.system, name, dimension, bright_overlay_path))
        return 1;

    LOG_WARN("stage", "Brightness " OVERLAY_NOP,
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension, step);
    return 0;
}

static void upload_texture_rgba(SDL_Surface *rgba, GLuint *out_tex) {
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    *out_tex = t;
}

static void disable_sdl(void) {
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (bright_sdl_tex[i]) {
            SDL_DestroyTexture(bright_sdl_tex[i]);
            bright_sdl_tex[i] = NULL;
        }

        bright_sdl_w[i] = 0;
        bright_sdl_h[i] = 0;
    }

    bright_disabled_sdl = 1;
    bright_preload_sdl_done = 1;
}

static void disable_gles(void) {
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (bright_gles_tex[i]) {
            glDeleteTextures(1, &bright_gles_tex[i]);
            bright_gles_tex[i] = 0;
        }

        bright_gles_w[i] = 0;
        bright_gles_h[i] = 0;
    }

    bright_disabled_gles = 1;
    bright_preload_gles_done = 1;

    vtx_bright_valid = 0;
}

static void preload_bright_textures_sdl(SDL_Renderer *renderer) {
    if (bright_preload_sdl_done || bright_disabled_sdl) return;
    if (!renderer) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_bright_path(RENDER_SDL, renderer, i)) {
            disable_sdl();
            return;
        }

        SDL_Surface *surface = IMG_Load(bright_overlay_path);
        if (!surface) {
            disable_sdl();
            return;
        }

        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (!tex) {
            disable_sdl();
            return;
        }

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_QueryTexture(tex, NULL, NULL, &bright_sdl_w[i], &bright_sdl_h[i]);

        bright_sdl_tex[i] = tex;
    }

    bright_preload_sdl_done = 1;
}

static void preload_bright_textures_gles(void) {
    if (bright_preload_gles_done || bright_disabled_gles) return;
    if (!render_window) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_bright_path(RENDER_GLES, render_window, i)) {
            disable_gles();
            return;
        }

        SDL_Surface *raw = IMG_Load(bright_overlay_path);
        if (!raw) {
            disable_gles();
            return;
        }

        SDL_Surface *rgba = raw;
        if (raw->format->format != SDL_PIXELFORMAT_RGBA32) {
            rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(raw);
            if (!rgba) {
                disable_gles();
                return;
            }
        }

        bright_gles_w[i] = rgba->w;
        bright_gles_h[i] = rgba->h;

        upload_texture_rgba(rgba, &bright_gles_tex[i]);
        SDL_FreeSurface(rgba);

        if (!bright_gles_tex[i]) {
            disable_gles();
            return;
        }
    }

    bright_preload_gles_done = 1;
}

void bright_overlay_init(void) {
    if (bright_init) return;

    memset(&bright_stat_last, 0, sizeof(bright_stat_last));
    bright_init = 1;

    static char bright_max[8];
    if (!read_line_from_file(INTERNAL_DEVICE "screen/bright", 1, bright_max, sizeof(bright_max))) {
        bright_device_max = 0;
        return;
    }

    bright_device_max = safe_atoi(bright_max);
}

void bright_overlay_update(void) {
    if (!bright_init) bright_overlay_init();
    if (bright_device_max <= 0) return;

    struct stat sb;
    if (stat(BRIGHT_PATH, &sb) != 0) return;

    int bright_pct;
    if (!read_percent(BRIGHT_PATH, bright_device_max, &bright_pct)) return;

    if (!bright_primed) {
        bright_primed = 1;
        bright_last_pct = bright_pct;
        bright_stat_last = sb;

        int step = bright_pct / 10;
        if (step < 0) step = 0;
        if (step > 9) step = 9;
        bright_last_step = step;

        return;
    }

    if (sb.st_mtime == bright_stat_last.st_mtime && bright_pct == bright_last_pct) return;

    bright_last_pct = bright_pct;
    bright_stat_last = sb;

    int step = bright_pct / 10;
    if (step < 0) step = 0;
    if (step > 9) step = 9;

    bright_last_step = step;

    if (bright_pct == 0) {
        bright_visible = 0;
        return;
    }

    bright_last_change_ms = now_ms();
    bright_visible = 1;
}

int bright_is_visible(void) {
    if (!bright_visible) return 0;

    if (now_ms() - bright_last_change_ms >= INDICATOR_SHOW_MS) {
        bright_visible = 0;
        return 0;
    }

    return 1;
}

void sdl_bright_overlay_init(SDL_Renderer *renderer) {
    if (bright_disabled_sdl) return;
    preload_bright_textures_sdl(renderer);
}

void gl_bright_overlay_init(void) {
    if (bright_disabled_gles) return;
    preload_bright_textures_gles();
}
