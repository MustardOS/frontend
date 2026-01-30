#include <unistd.h>
#include <SDL2/SDL.h>
#include "../common/common.h"
#include "../../common/inotify.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/rotate.h"
#include "../common/scale.h"
#include "../common/stretch.h"
#include "../overlay/base.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../hook.h"

static SDL_Texture *content_tex = NULL;
static int content_w = 0;
static int content_h = 0;

static int content_rot_cached = ROTATE_0;
static int content_fb_w = 0;
static int content_fb_h = 0;

static uint32_t last_capture_ms = 0;
static uint32_t capture_interval_ms = 0;

static inline int capture_content_tex(void) {
    if (capture_interval_ms == 0) return 1;
    uint32_t now = SDL_GetTicks();

    if ((uint32_t) (now - last_capture_ms) >= capture_interval_ms) {
        last_capture_ms = now;
        return 1;
    }

    return 0;
}

static void destroy_content_tex(void) {
    if (content_tex) {
        SDL_DestroyTexture(content_tex);
        content_tex = NULL;
    }

    content_w = 0;
    content_h = 0;

    last_capture_ms = 0;
}

static void ensure_content_tex(SDL_Renderer *r, int w, int h) {
    if (content_tex && w == content_w && h == content_h) return;
    destroy_content_tex();

    content_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);

    if (!content_tex) return;
    SDL_SetTextureBlendMode(content_tex, SDL_BLENDMODE_NONE);

    content_w = w;
    content_h = h;
}

static void draw_rotated_content(SDL_Renderer *r, int rot, int fb_w, int fb_h) {
    if (rot == ROTATE_0) return;
    if (fb_w < 1 || fb_h < 1) return;

    ensure_content_tex(r, fb_w, fb_h);
    if (!content_tex) return;

    void *pixels = NULL;
    int pitch = 0;

    if (capture_content_tex()) {
        if (SDL_LockTexture(content_tex, NULL, &pixels, &pitch) != 0)
            return;

        if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, pixels, pitch) != 0) {
            SDL_UnlockTexture(content_tex);
            return;
        }

        SDL_UnlockTexture(content_tex);
    }

    static SDL_Rect dst;
    static int dst_rot = ROTATE_0;
    static int dst_fb_w = 0;
    static int dst_fb_h = 0;
    static double angle = 0.0;

    if (rot != dst_rot || fb_w != dst_fb_w || fb_h != dst_fb_h) {
        if (rot == ROTATE_90 || rot == ROTATE_270) {
            dst.w = fb_h;
            dst.h = fb_w;
            dst.x = (fb_w - dst.w) / 2;
            dst.y = (fb_h - dst.h) / 2;
        } else {
            dst.w = fb_w;
            dst.h = fb_h;
            dst.x = 0;
            dst.y = 0;
        }

        angle = rotate_angle(rot);

        dst_rot = rot;
        dst_fb_w = fb_w;
        dst_fb_h = fb_h;
    }

    SDL_RenderCopyEx(r, content_tex, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
}

static void set_sdl_render(SDL_Renderer *renderer, int tex_w, int tex_h, int anchor, int scale, SDL_Rect *out) {
    int fb_w, fb_h;
    SDL_GetRendererOutputSize(renderer, &fb_w, &fb_h);

    const int rot = rotate_read_cached();

    int draw_w = tex_w;
    int draw_h = tex_h;
    stretch_draw_size(tex_w, tex_h, fb_w, fb_h, scale, rot, &draw_w, &draw_h);

    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    int x = 0;
    int y = 0;

    switch (get_anchor_rotate(anchor, rot)) {
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

static void draw_sdl_overlay(SDL_Renderer *renderer, SDL_Texture *tex, int tex_w, int tex_h,
                             float alpha, int anchor, int scale) {
    if (!renderer || !tex) return;

    SDL_Rect dst;
    set_sdl_render(renderer, tex_w, tex_h, anchor, scale, &dst);

    const int rot = rotate_read_cached();
    const double angle = rotate_angle(rot);

    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, (Uint8) (alpha * 255.0f));
    SDL_RenderCopyEx(renderer, tex, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    if (is_overlay_disabled()) {
        if (real_SDL_RenderPresent) real_SDL_RenderPresent(renderer);
        return;
    }

    if (!real_SDL_RenderPresent) return;

    base_inotify_check();
    if (ino_proc) inotify_check(ino_proc);

    static SDL_Renderer *last_renderer = NULL;
    if (renderer != last_renderer) {
        destroy_content_tex();

        content_rot_cached = ROTATE_0;
        content_fb_w = 0;
        content_fb_h = 0;

        base_sdl_ready = 0;
        sdl_overlay_resolved = 0;
        sdl_overlay_path_last[0] = '\0';
        base_nop_last = -1;

        last_renderer = renderer;
    }

    const int rot = rotate_read_cached();
    capture_interval_ms = rot == ROTATE_0 ? 0 : 33;

    int fb_w = 0, fb_h = 0;
    SDL_GetRendererOutputSize(renderer, &fb_w, &fb_h);

    if (rot != content_rot_cached || fb_w != content_fb_w || fb_h != content_fb_h) {
        destroy_content_tex();

        content_rot_cached = rot;
        content_fb_w = fb_w;
        content_fb_h = fb_h;
    }

    const int base_disabled = ino_proc ? base_overlay_disabled_cached : (access(BASE_OVERLAY_NOP, F_OK) == 0);
    if (base_disabled != base_nop_last) {
        if (base_disabled) {
            if (base_sdl_tex) {
                SDL_DestroyTexture(base_sdl_tex);
                base_sdl_tex = NULL;
            }
            base_sdl_ready = 0;
            sdl_overlay_resolved = 0;
            sdl_overlay_path_last[0] = '\0';
        }
        base_nop_last = base_disabled;
    }

    bright_overlay_init();
    volume_overlay_init();

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();

    if (rot != ROTATE_0) draw_rotated_content(renderer, rot, fb_w, fb_h);

    if (!base_disabled) {
        sdl_base_overlay_init(renderer);
        if (base_sdl_tex) {
            draw_sdl_overlay(renderer, base_sdl_tex,
                             base_sdl_w, base_sdl_h,
                             get_alpha_cached(&overlay_alpha_cache),
                             get_anchor_cached(&overlay_anchor_cache),
                             get_scale_cached(&overlay_scale_cache));
        }
    }

    // We place the battery overlay init here because we immediately
    // want to show the low battery indicator when content starts!
    sdl_battery_overlay_init(renderer);
    int battery_step = battery_last_step;
    if (battery_step >= 0 && battery_step < INDICATOR_STEPS && battery_sdl_tex[battery_step]) {
        draw_sdl_overlay(renderer, battery_sdl_tex[battery_step],
                         battery_sdl_w[battery_step], battery_sdl_h[battery_step],
                         get_alpha_cached(&battery_alpha_cache),
                         get_anchor_cached(&battery_anchor_cache),
                         get_scale_cached(&battery_scale_cache));
    }

    if (bright_is_visible()) {
        sdl_bright_overlay_init(renderer);
        int step = bright_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && bright_sdl_tex[step]) {
            draw_sdl_overlay(renderer, bright_sdl_tex[step],
                             bright_sdl_w[step], bright_sdl_h[step],
                             get_alpha_cached(&bright_alpha_cache),
                             get_anchor_cached(&bright_anchor_cache),
                             get_scale_cached(&bright_scale_cache));
        }
    }

    if (volume_is_visible()) {
        sdl_volume_overlay_init(renderer);
        int step = volume_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && volume_sdl_tex[step]) {
            draw_sdl_overlay(renderer, volume_sdl_tex[step],
                             volume_sdl_w[step], volume_sdl_h[step],
                             get_alpha_cached(&volume_alpha_cache),
                             get_anchor_cached(&volume_anchor_cache),
                             get_scale_cached(&volume_scale_cache));
        }
    }

    real_SDL_RenderPresent(renderer);
}
