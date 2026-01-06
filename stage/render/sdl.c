#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "../overlay/battery.h"
#include "../hook.h"
#include "sdl.h"

static SDL_Texture *sdl_tex;
static int sdl_ready;
static int sdl_attempted;
static int sdl_tex_w;
static int sdl_tex_h;

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
    if (sdl_ready) return;

    if (sdl_attempted) return;
    sdl_attempted = 1;

    if (!load_overlay_common(&SDL_RESOLVER, renderer, overlay_path)) {
        LOG_WARN("stage", "SDL overlay failed to resolve path");
        return;
    }

    SDL_Surface *surface = IMG_Load(overlay_path);
    if (!surface) {
        LOG_ERROR("stage", "SDL Image load failed: %s", IMG_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    sdl_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!sdl_tex) {
        LOG_ERROR("stage", "SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
        return;
    }

    SDL_SetTextureBlendMode(sdl_tex, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(sdl_tex, NULL, NULL, &sdl_tex_w, &sdl_tex_h);

    SDL_FreeSurface(surface);
    sdl_ready = 1;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    if (!real_SDL_RenderPresent) return;

    battery_overlay_update();

    float overlay_alpha = get_alpha_cached(&overlay_alpha_cache);
    float battery_alpha = get_alpha_cached(&battery_alpha_cache);

    sdl_overlay_init(renderer);

    if (sdl_tex) {
        int overlay_anchor = get_anchor_cached(&overlay_anchor_cache);
        int overlay_scale  = get_scale_cached(&overlay_scale_cache);

        SDL_Rect overlay_dst;
        set_sdl_render(renderer, sdl_tex_w, sdl_tex_h, overlay_anchor, overlay_scale, &overlay_dst);

        SDL_SetTextureColorMod(sdl_tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(sdl_tex, (Uint8)(overlay_alpha * 255.0f));
        SDL_RenderCopy(renderer, sdl_tex, NULL, &overlay_dst);
    }

    sdl_battery_overlay_init(renderer);

    if (battery_sdl_tex) {
        int battery_anchor = get_anchor_cached(&battery_anchor_cache);
        int battery_scale  = get_scale_cached(&battery_scale_cache);

        SDL_Rect battery_dst;
        set_sdl_render(renderer, battery_sdl_w, battery_sdl_h, battery_anchor, battery_scale, &battery_dst);

        SDL_SetTextureColorMod(battery_sdl_tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(battery_sdl_tex, (Uint8)(battery_alpha * 255.0f));
        SDL_RenderCopy(renderer, battery_sdl_tex, NULL, &battery_dst);
    }

    real_SDL_RenderPresent(renderer);
}
