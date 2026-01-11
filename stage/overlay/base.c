#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "base.h"

int base_anchor_cached = -1;
int base_scale_cached = -1;

SDL_Texture *base_sdl_tex;
int base_sdl_ready;
int base_sdl_w;
int base_sdl_h;

GLuint base_gles_tex = 0;
int base_gles_w = 0;
int base_gles_h = 0;
int base_gles_attempted = 0;
int base_gles_ready = 0;

gl_vtx_t vtx_base[4];
int vtx_base_valid = 0;

int sdl_overlay_resolved = 0;
char sdl_overlay_path_last[PATH_MAX];
uint64_t sdl_overlay_last_check_ms = 0;

uint64_t base_nop_last_check_ms = 0;
int base_nop_cached = 0;

struct alpha_cache base_alpha_cache = {
        .path  = BASE_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

struct anchor_cache base_anchor_cache = {
        .path  = BASE_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_CENTRE_MIDDLE
};

struct scale_cache base_scale_cache = {
        .path  = BASE_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

const struct overlay_resolver GLES_RESOLVER = {
        .render_method = RENDER_GLES,
        .get_dimension = get_dimension,
};

const struct overlay_resolver SDL_RESOLVER = {
        .render_method = RENDER_SDL,
        .get_dimension = get_dimension,
};

int base_overlay_disabled(void) {
    uint64_t now = now_ms();
    if (now - base_nop_last_check_ms < 200) return base_nop_cached;
    base_nop_last_check_ms = now;

    base_nop_cached = (access(BASE_OVERLAY_NOP, F_OK) == 0);
    return base_nop_cached;
}

void destroy_base_gles(void) {
    if (base_gles_tex) {
        glDeleteTextures(1, &base_gles_tex);
        base_gles_tex = 0;
    }

    base_gles_w = 0;
    base_gles_h = 0;
    base_gles_ready = 0;
    base_gles_attempted = 0;

    vtx_base_valid = 0;
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

static int read_overlay_loader(struct overlay_go_cache *cache) {
    struct stat st;

    if (stat(OVERLAY_LOADER, &st) != 0) {
        cache->valid = 0;
        return 0;
    }

    if (cache->valid && st.st_mtime == cache->mtime) return 1;
    cache->mtime = st.st_mtime;
    cache->valid = 0;

    if (!read_line_from_file(OVERLAY_LOADER, 1, cache->content, sizeof(cache->content))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 2, cache->system, sizeof(cache->system))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 3, cache->core, sizeof(cache->core))) return 0;

    cache->valid = 1;
    return 1;
}

static int resolve_base_overlay(const struct overlay_resolver *res, void *ctx, char *overlay_path) {
    res->get_dimension(res->render_method, ctx, dimension, sizeof(dimension));
    if (!read_overlay_loader(&ovl_go_cache)) return 0;

    if (load_stage_image("base", ovl_go_cache.core, ovl_go_cache.system,
                         ovl_go_cache.content, dimension, overlay_path)) {
        LOG_SUCCESS("stage", "Overlay loaded: %s", overlay_path);
        return 1;
    }

    LOG_WARN("stage", "Overlay not found (core=%s, system=%s, content=%s, dim=%s)",
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension);
    return 0;
}

void sdl_base_overlay_init(SDL_Renderer *renderer) {
    if (!renderer) return;

    char wanted_overlay[PATH_MAX];
    wanted_overlay[0] = '\0';

    if (!sdl_overlay_resolved) {
        if (!resolve_base_overlay(&SDL_RESOLVER, renderer, wanted_overlay)) return;
        sdl_overlay_resolved = 1;
    } else {
        strlcpy(wanted_overlay, sdl_overlay_path_last, sizeof(wanted_overlay));
    }

    if (!wanted_overlay[0]) return;
    if (base_sdl_ready && strcmp(sdl_overlay_path_last, wanted_overlay) == 0) return;

    uint64_t now = now_ms();
    if (now - sdl_overlay_last_check_ms < 200) return;
    sdl_overlay_last_check_ms = now;

    SDL_Surface *surface = IMG_Load(wanted_overlay);
    if (!surface) {
        LOG_ERROR("stage", "SDL Image load failed: %s", IMG_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    SDL_Texture *new_tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!new_tex) {
        LOG_ERROR("stage", "SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return;
    }

    int w, h;

    SDL_SetTextureBlendMode(new_tex, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(new_tex, NULL, NULL, &w, &h);

    if (base_sdl_tex) SDL_DestroyTexture(base_sdl_tex);

    base_sdl_tex = new_tex;
    base_sdl_w = w;
    base_sdl_h = h;
    base_sdl_ready = 1;

    strlcpy(sdl_overlay_path_last, wanted_overlay, sizeof(sdl_overlay_path_last));
}

void gl_base_overlay_init(SDL_Window *window) {
    if (base_gles_ready || base_gles_attempted) return;
    base_gles_attempted = 1;

    if (!window || !resolve_base_overlay(&GLES_RESOLVER, window, overlay_path)) return;

    SDL_Surface *raw = IMG_Load(overlay_path);
    if (!raw) {
        LOG_ERROR("stage", "GL overlay image load failed: %s", IMG_GetError());
        return;
    }

    SDL_Surface *rgba = raw;
    if (raw->format->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(raw);
        raw = NULL;
        if (!rgba) {
            LOG_ERROR("stage", "SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
            return;
        }
    }

    base_gles_w = rgba->w;
    base_gles_h = rgba->h;

    upload_texture_rgba(rgba, &base_gles_tex);
    if (!base_gles_tex) {
        SDL_FreeSurface(rgba);
        return;
    }

    SDL_FreeSurface(rgba);

    base_gles_ready = 1;
    vtx_base_valid = 0;
}
