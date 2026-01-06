#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "battery.h"

char battery_overlay_path[512];
int battery_last_enabled = -1;

int battery_anchor_cached = -1;
int battery_scale_cached = -1;

SDL_Texture *battery_sdl_tex;
int battery_sdl_ready;
int battery_sdl_attempted;
int battery_sdl_w;
int battery_sdl_h;

GLuint battery_gles_tex;
int battery_gles_ready;
int battery_gles_attempted;
int battery_gles_w;
int battery_gles_h;

gl_vtx_t vtx_battery[4];
int vtx_battery_valid = 0;

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

void battery_overlay_update(void) {
    int enabled = battery_overlay_enabled();
    if (enabled == battery_last_enabled) return;

    battery_last_enabled = enabled;
    if (enabled) {
        battery_sdl_attempted = 0;
        battery_gles_attempted = 0;
        return;
    }

    if (battery_sdl_tex) {
        SDL_DestroyTexture(battery_sdl_tex);
        battery_sdl_tex = NULL;
    }

    if (battery_gles_tex) {
        glDeleteTextures(1, &battery_gles_tex);
        battery_gles_tex = 0;
    }

    battery_anchor_cache.mtime = 0;
    battery_alpha_cache.mtime = 0;
    battery_scale_cache.mtime = 0;

    battery_anchor_cached = -1;
    battery_scale_cached = -1;

    battery_sdl_ready = 0;
    battery_gles_ready = 0;

    vtx_battery_valid = 0;
    battery_overlay_path[0] = '\0';
}

static int resolve_battery_overlay(enum render_method type, void *ctx) {
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

    if (load_stage_image("battery", ovl_go_cache.core, ovl_go_cache.system,
                           BATTERY_IMAGE, dimension, battery_overlay_path)) {
        LOG_SUCCESS("stage", "Battery overlay loaded: %s", battery_overlay_path);
        return 1;
    }

    LOG_WARN("stage", "Battery overlay not found (core=%s, system=%s, content=%s, dim=%s)",
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension);
    return 0;
}

void sdl_battery_overlay_init(SDL_Renderer *renderer) {
    if (battery_sdl_ready || battery_sdl_attempted) return;
    battery_sdl_attempted = 1;

    if (!battery_overlay_enabled()) return;
    if (!resolve_battery_overlay(RENDER_SDL, renderer))return;

    SDL_Surface *surface = IMG_Load(battery_overlay_path);
    if (!surface) {
        LOG_ERROR("stage", "Battery PNG load failed: %s", IMG_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    battery_sdl_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!battery_sdl_tex) {
        LOG_ERROR("stage", "Battery texture create failed: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_SetTextureBlendMode(battery_sdl_tex, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(battery_sdl_tex, NULL, NULL, &battery_sdl_w, &battery_sdl_h);

    SDL_FreeSurface(surface);
    battery_sdl_ready = 1;
}

void gl_battery_overlay_init(void) {
    if (battery_gles_ready || battery_gles_attempted) return;
    battery_gles_attempted = 1;

    if (!battery_overlay_enabled()) return;
    if (!resolve_battery_overlay(RENDER_GLES, render_window)) return;

    SDL_Surface *raw = IMG_Load(battery_overlay_path);
    if (!raw) {
        LOG_ERROR("stage", "Battery GL image load failed: %s", IMG_GetError());
        return;
    }

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);

    if (!rgba) return;

    battery_gles_w = rgba->w;
    battery_gles_h = rgba->h;

    glGenTextures(1, &battery_gles_tex);
    glBindTexture(GL_TEXTURE_2D, battery_gles_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    SDL_FreeSurface(rgba);
    battery_gles_ready = 1;
}
