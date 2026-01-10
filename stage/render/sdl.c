#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../hook.h"
#include "sdl.h"

static SDL_Texture *sdl_tex;

static int sdl_ready;
static int sdl_tex_w;
static int sdl_tex_h;

static int sdl_overlay_resolved = 0;
static char sdl_overlay_path_last[PATH_MAX];
static uint64_t sdl_overlay_last_check_ms = 0;

static void set_sdl_render(SDL_Renderer *renderer, int tex_w, int tex_h, int anchor, int scale, SDL_Rect *out) {
    int fb_w, fb_h;
    SDL_GetRendererOutputSize(renderer, &fb_w, &fb_h);

    int draw_w = tex_w;
    int draw_h = tex_h;

    if (scale == SCALE_FIT) {
        float sx = (float) fb_w / (float) tex_w;
        float sy = (float) fb_h / (float) tex_h;

        float s = (sx < sy) ? sx : sy;

        draw_w = (int) lroundf((float) tex_w * s);
        draw_h = (int) lroundf((float) tex_h * s);
    } else if (scale == SCALE_STRETCH) {
        draw_w = fb_w;
        draw_h = fb_h;
    }

    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    int x = 0;
    int y = 0;

    switch (anchor) {
        case ANCHOR_TOP_LEFT:
            x = 0;
            y = 0;
            break;
        case ANCHOR_TOP_MIDDLE:
            x = (fb_w - draw_w) / 2;
            break;
        case ANCHOR_TOP_RIGHT:
            x = fb_w - draw_w;
            break;
        case ANCHOR_CENTRE_LEFT:
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x = (fb_w - draw_w) / 2;
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x = fb_w - draw_w;
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_BOTTOM_LEFT:
            y = fb_h - draw_h;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x = (fb_w - draw_w) / 2;
            y = fb_h - draw_h;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x = fb_w - draw_w;
            y = fb_h - draw_h;
            break;
        default:
            x = (fb_w - draw_w) / 2;
            y = (fb_h - draw_h) / 2;
            break;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    out->x = x;
    out->y = y;
    out->w = draw_w;
    out->h = draw_h;
}

static void sdl_overlay_init(SDL_Renderer *renderer) {
    if (!renderer) return;

    char wanted_overlay[PATH_MAX];
    wanted_overlay[0] = '\0';

    if (!sdl_overlay_resolved) {
        if (!load_overlay_common(&SDL_RESOLVER, renderer, wanted_overlay)) return;
        sdl_overlay_resolved = 1;
    } else {
        strlcpy(wanted_overlay, sdl_overlay_path_last, sizeof(wanted_overlay));
    }

    if (!wanted_overlay[0]) return;
    if (sdl_ready && strcmp(sdl_overlay_path_last, wanted_overlay) == 0) return;

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

    if (sdl_tex) SDL_DestroyTexture(sdl_tex);

    sdl_tex = new_tex;
    sdl_tex_w = w;
    sdl_tex_h = h;
    sdl_ready = 1;

    strlcpy(sdl_overlay_path_last, wanted_overlay, sizeof(sdl_overlay_path_last));
}

static void draw_sdl_overlay(SDL_Renderer *renderer, SDL_Texture *tex, int tex_w, int tex_h,
                             float alpha, int anchor, int scale) {
    if (!renderer || !tex) return;

    SDL_Rect dst;
    set_sdl_render(renderer, tex_w, tex_h, anchor, scale, &dst);

    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, (Uint8) (alpha * 255.0f));
    SDL_RenderCopy(renderer, tex, NULL, &dst);
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    if (is_overlay_disabled()) {
        if (real_SDL_RenderPresent) real_SDL_RenderPresent(renderer);
        return;
    }

    if (!real_SDL_RenderPresent) return;

    static SDL_Renderer *last_renderer;
    if (renderer != last_renderer) {
        sdl_ready = 0;
        sdl_overlay_resolved = 0;
        sdl_overlay_path_last[0] = '\0';
        last_renderer = renderer;
    }

    bright_overlay_init();
    volume_overlay_init();

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();

    sdl_overlay_init(renderer);

    if (sdl_tex) {
        draw_sdl_overlay(renderer, sdl_tex, sdl_tex_w, sdl_tex_h,
                         get_alpha_cached(&overlay_alpha_cache),
                         get_anchor_cached(&overlay_anchor_cache),
                         get_scale_cached(&overlay_scale_cache)
        );
    }

    // We place the battery overlay init here because we immediately
    // want to show the low battery indicator when content starts!
    sdl_battery_overlay_init(renderer);
    if (battery_sdl_tex) {
        draw_sdl_overlay(renderer, battery_sdl_tex, battery_sdl_w, battery_sdl_h,
                         get_alpha_cached(&battery_alpha_cache),
                         get_anchor_cached(&battery_anchor_cache),
                         get_scale_cached(&battery_scale_cache)
        );
    }

    if (bright_is_visible()) {
        sdl_bright_overlay_init(renderer);
        int step = bright_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && bright_sdl_tex[step]) {
            draw_sdl_overlay(renderer, bright_sdl_tex[step], bright_sdl_w[step], bright_sdl_h[step],
                             get_alpha_cached(&bright_alpha_cache),
                             get_anchor_cached(&bright_anchor_cache),
                             get_scale_cached(&bright_scale_cache)
            );
        }
    }

    if (volume_is_visible()) {
        sdl_volume_overlay_init(renderer);
        int step = volume_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && volume_sdl_tex[step]) {
            draw_sdl_overlay(renderer, volume_sdl_tex[step], volume_sdl_w[step], volume_sdl_h[step],
                             get_alpha_cached(&volume_alpha_cache),
                             get_anchor_cached(&volume_anchor_cache),
                             get_scale_cached(&volume_scale_cache)
            );
        }
    }

    real_SDL_RenderPresent(renderer);
}
