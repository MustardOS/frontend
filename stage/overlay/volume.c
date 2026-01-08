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
#include "volume.h"

char volume_overlay_path[PATH_MAX];

struct stat volume_stat_last;
int volume_init = 0;
int volume_primed = 0;
int volume_visible = 0;
int volume_last_step = -1;
int volume_last_pct = -1;
int volume_device_max = 0;
uint64_t volume_last_change_ms = 0;

int volume_anchor_cached = -1;
int volume_scale_cached = -1;

SDL_Texture *volume_sdl_tex[INDICATOR_STEPS];
int volume_sdl_w[INDICATOR_STEPS];
int volume_sdl_h[INDICATOR_STEPS];

GLuint volume_gles_tex[INDICATOR_STEPS];
int volume_gles_w[INDICATOR_STEPS];
int volume_gles_h[INDICATOR_STEPS];

int volume_preload_sdl_done = 0;
int volume_preload_gles_done = 0;

int volume_disabled_sdl = 0;
int volume_disabled_gles = 0;

gl_vtx_t vtx_volume[4];
int vtx_volume_valid = 0;

struct alpha_cache volume_alpha_cache = {
        .path  = VOLUME_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

struct anchor_cache volume_anchor_cache = {
        .path  = VOLUME_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_BOTTOM_MIDDLE
};

struct scale_cache volume_scale_cache = {
        .path  = VOLUME_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static int ensure_volume_path(enum render_method type, void *ctx, int step) {
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
    snprintf(name, sizeof(name), "volume_%d", step);

    if (load_stage_image("volume", ovl_go_cache.core, ovl_go_cache.system, name, dimension, volume_overlay_path))
        return 1;

    LOG_WARN("stage", "Volume " OVERLAY_NOP,
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
        if (volume_sdl_tex[i]) {
            SDL_DestroyTexture(volume_sdl_tex[i]);
            volume_sdl_tex[i] = NULL;
        }

        volume_sdl_w[i] = 0;
        volume_sdl_h[i] = 0;
    }

    volume_disabled_sdl = 1;
    volume_preload_sdl_done = 1;
}

static void disable_gles(void) {
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (volume_gles_tex[i]) {
            glDeleteTextures(1, &volume_gles_tex[i]);
            volume_gles_tex[i] = 0;
        }

        volume_gles_w[i] = 0;
        volume_gles_h[i] = 0;
    }

    volume_disabled_gles = 1;
    volume_preload_gles_done = 1;

    vtx_volume_valid = 0;
}

static void preload_volume_textures_sdl(SDL_Renderer *renderer) {
    if (volume_preload_sdl_done || volume_disabled_sdl) return;
    if (!renderer) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_volume_path(RENDER_SDL, renderer, i)) {
            disable_sdl();
            return;
        }

        SDL_Surface *surface = IMG_Load(volume_overlay_path);
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
        SDL_QueryTexture(tex, NULL, NULL, &volume_sdl_w[i], &volume_sdl_h[i]);

        volume_sdl_tex[i] = tex;
    }

    volume_preload_sdl_done = 1;
}

static void preload_volume_textures_gles(void) {
    if (volume_preload_gles_done || volume_disabled_gles) return;
    if (!render_window) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_volume_path(RENDER_GLES, render_window, i)) {
            disable_gles();
            return;
        }

        SDL_Surface *raw = IMG_Load(volume_overlay_path);
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

        volume_gles_w[i] = rgba->w;
        volume_gles_h[i] = rgba->h;

        upload_texture_rgba(rgba, &volume_gles_tex[i]);
        SDL_FreeSurface(rgba);

        if (!volume_gles_tex[i]) {
            disable_gles();
            return;
        }
    }

    volume_preload_gles_done = 1;
}

void volume_overlay_init(void) {
    if (volume_init) return;

    memset(&volume_stat_last, 0, sizeof(volume_stat_last));
    volume_init = 1;

    static char volume_max[8];
    if (!read_line_from_file(INTERNAL_DEVICE "audio/max", 1, volume_max, sizeof(volume_max))) {
        volume_device_max = 0;
        return;
    }

    volume_device_max = safe_atoi(volume_max);
}

void volume_overlay_update(void) {
    if (!volume_init) volume_overlay_init();
    if (volume_device_max <= 0) return;

    struct stat sv;
    if (stat(VOLUME_PATH, &sv) != 0) return;

    int volume_pct;
    if (!read_percent(VOLUME_PATH, volume_device_max, &volume_pct)) return;

    if (!volume_primed) {
        volume_primed = 1;
        volume_last_pct = volume_pct;
        volume_stat_last = sv;

        int step = volume_pct / 10;
        if (step < 0) step = 0;
        if (step > 9) step = 9;
        volume_last_step = step;

        return;
    }

    if (sv.st_mtime == volume_stat_last.st_mtime && volume_pct == volume_last_pct) return;

    volume_last_pct = volume_pct;
    volume_stat_last = sv;

    int step = volume_pct / 10;
    if (step < 0) step = 0;
    if (step > 9) step = 9;

    volume_last_step = step;

    if (volume_pct == 0) {
        volume_visible = 0;
        return;
    }

    volume_last_change_ms = now_ms();
    volume_visible = 1;
}

int volume_is_visible(void) {
    if (!volume_visible) return 0;

    if (now_ms() - volume_last_change_ms >= INDICATOR_SHOW_MS) {
        volume_visible = 0;
        return 0;
    }

    return 1;
}

void sdl_volume_overlay_init(SDL_Renderer *renderer) {
    if (volume_disabled_sdl) return;
    preload_volume_textures_sdl(renderer);
}

void gl_volume_overlay_init(void) {
    if (volume_disabled_gles) return;
    preload_volume_textures_gles();
}
