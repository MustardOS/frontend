#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/rotate.h"
#include "../common/scale.h"
#include "battery.h"

char battery_overlay_path[PATH_MAX];

static struct stat battery_stat_last;
static int battery_last_enabled = -1;
static int battery_primed = 0;
static int battery_last_pct = -1;

int battery_last_step = -1;

int battery_anchor_cached = -1;
int battery_scale_cached = -1;
int battery_rotate_cached = ROTATE_0;

SDL_Texture *battery_sdl_tex[INDICATOR_STEPS];
int battery_sdl_w[INDICATOR_STEPS];
int battery_sdl_h[INDICATOR_STEPS];

GLuint battery_gles_tex[INDICATOR_STEPS];
int battery_gles_w[INDICATOR_STEPS];
int battery_gles_h[INDICATOR_STEPS];

int battery_preload_sdl_done = 0;
int battery_preload_gles_done = 0;

int battery_disabled_sdl = 0;
int battery_disabled_gles = 0;

gl_vtx_t vtx_battery[4];
int vtx_battery_valid = 0;

struct offset_cache battery_offset_cache = {
        .path  = BATTERY_OFFSET,
        .mtime = 0,
        .value = 0
};

struct flag_cache battery_enable_cache = {
        .path  = BATTERY_DETECT,
        .mtime = 0,
        .value = 0
};

struct alpha_cache battery_alpha_cache = {
        .path  = BATTERY_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

struct anchor_cache battery_anchor_cache = {
        .path  = BATTERY_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_TOP_LEFT
};

struct scale_cache battery_scale_cache = {
        .path  = BATTERY_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static int get_battery_offset(void) {
    struct stat st;

    if (stat(battery_offset_cache.path, &st) != 0) {
        battery_offset_cache.mtime = 0;
        battery_offset_cache.value = 0;
        return 0;
    }

    if (st.st_mtime == battery_offset_cache.mtime) return battery_offset_cache.value;

    char buf[16];
    int v = 0;

    if (read_line_from_file(battery_offset_cache.path, 1, buf, sizeof(buf))) v = safe_atoi(buf);

    if (v < -50) v = -50;
    if (v > 50) v = 50;

    battery_offset_cache.mtime = st.st_mtime;
    battery_offset_cache.value = v;

    return v;
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
        if (battery_sdl_tex[i]) {
            SDL_DestroyTexture(battery_sdl_tex[i]);
            battery_sdl_tex[i] = NULL;
        }

        battery_sdl_w[i] = 0;
        battery_sdl_h[i] = 0;
    }

    battery_disabled_sdl = 1;
    battery_preload_sdl_done = 1;
}

static void disable_gles(void) {
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (battery_gles_tex[i]) {
            glDeleteTextures(1, &battery_gles_tex[i]);
            battery_gles_tex[i] = 0;
        }

        battery_gles_w[i] = 0;
        battery_gles_h[i] = 0;
    }

    battery_disabled_gles = 1;
    battery_preload_gles_done = 1;
    vtx_battery_valid = 0;
}

int battery_overlay_enabled(void) {
    struct stat st;

    if (stat(battery_enable_cache.path, &st) != 0) {
        battery_enable_cache.mtime = 0;
        battery_enable_cache.value = 0;
        return 0;
    }

    if (st.st_mtime == battery_enable_cache.mtime) return battery_enable_cache.value;

    battery_enable_cache.mtime = st.st_mtime;
    battery_enable_cache.value = 1;

    return 1;
}

static int ensure_battery_path(enum render_method type, void *ctx, int step) {
    if (!ctx) return 0;

    char dimension[32];
    battery_overlay_path[0] = '\0';

    switch (type) {
        case RENDER_SDL:
            get_dimension(RENDER_SDL, ctx, dimension, sizeof(dimension));
            break;
        case RENDER_GLES:
            get_dimension(RENDER_GLES, ctx, dimension, sizeof(dimension));
            break;
    }

    char name[64];
    snprintf(name, sizeof(name), "battery_%d", step);

    if (load_stage_image("battery", ovl_go_cache.core, ovl_go_cache.system, name, dimension, battery_overlay_path))
        return 1;

    LOG_WARN("stage", "Battery " OVERLAY_NOP,
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension, step);
    return 0;
}

static void preload_battery_textures_sdl(SDL_Renderer *renderer) {
    if (battery_preload_sdl_done || battery_disabled_sdl || !renderer) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_battery_path(RENDER_SDL, renderer, i)) {
            disable_sdl();
            return;
        }

        SDL_Surface *surface = IMG_Load(battery_overlay_path);
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
        SDL_QueryTexture(tex, NULL, NULL, &battery_sdl_w[i], &battery_sdl_h[i]);

        battery_sdl_tex[i] = tex;
    }

    battery_preload_sdl_done = 1;
}

static void preload_battery_textures_gles(void) {
    if (battery_preload_gles_done || battery_disabled_gles || !render_window) return;

    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (!ensure_battery_path(RENDER_GLES, render_window, i)) {
            disable_gles();
            return;
        }

        SDL_Surface *raw = IMG_Load(battery_overlay_path);
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

        battery_gles_w[i] = rgba->w;
        battery_gles_h[i] = rgba->h;

        upload_texture_rgba(rgba, &battery_gles_tex[i]);
        SDL_FreeSurface(rgba);

        if (!battery_gles_tex[i]) {
            disable_gles();
            return;
        }
    }

    battery_preload_gles_done = 1;
}

static void reset_runtime_state(void) {
    battery_anchor_cache.mtime = 0;
    battery_alpha_cache.mtime = 0;
    battery_scale_cache.mtime = 0;
    battery_offset_cache.mtime = 0;

    battery_anchor_cached = -1;
    battery_scale_cached = -1;

    vtx_battery_valid = 0;

    battery_overlay_path[0] = '\0';

    memset(&battery_stat_last, 0, sizeof(battery_stat_last));
    battery_primed = 0;
    battery_last_pct = -1;
    battery_last_step = -1;
}

void battery_overlay_update(void) {
    int enabled = battery_overlay_enabled();

    if (enabled != battery_last_enabled) {
        battery_last_enabled = enabled;

        if (!enabled) {
            disable_sdl();
            disable_gles();

            battery_disabled_sdl = 0;
            battery_disabled_gles = 0;
            battery_preload_sdl_done = 0;
            battery_preload_gles_done = 0;

            reset_runtime_state();
            return;
        }

        reset_runtime_state();
    }

    if (!enabled) return;

    // Get the battery from our device config area
    char dev_battery[128];
    if (!read_line_from_file(INTERNAL_DEVICE "battery/capacity", 1, dev_battery, sizeof(dev_battery))) return;

    // Track battery % changes without polling too hard
    struct stat sv;
    if (stat(dev_battery, &sv) != 0) return;

    char pct_buf[16];
    if (!read_line_from_file(dev_battery, 1, pct_buf, sizeof(pct_buf))) return;

    int pct = safe_atoi(pct_buf);
    int offset = get_battery_offset();

    pct += offset;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    if (!battery_primed) {
        battery_primed = 1;
        battery_last_pct = pct;
        battery_stat_last = sv;

        int step = pct / 10;
        if (step < 0) step = 0;
        if (step > 9) step = 9;
        battery_last_step = step;
        return;
    }

    if (sv.st_mtime == battery_stat_last.st_mtime && pct == battery_last_pct) return;

    battery_last_pct = pct;
    battery_stat_last = sv;

    int step = pct / 10;
    if (step < 0) step = 0;
    if (step > 9) step = 9;

    if (step != battery_last_step) {
        battery_last_step = step;
        vtx_battery_valid = 0;
    }
}

void sdl_battery_overlay_init(SDL_Renderer *renderer) {
    if (battery_disabled_sdl) return;
    if (!battery_overlay_enabled()) return;
    preload_battery_textures_sdl(renderer);
}

void gl_battery_overlay_init(void) {
    if (battery_disabled_gles) return;
    if (!battery_overlay_enabled()) return;
    preload_battery_textures_gles();
}
