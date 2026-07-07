#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../common/device.h"
#include "../common/display.h"
#include "../common/init.h"
#include "../common/log.h"
#include "muxretro.h"
#include "settings.h"

static SDL_Texture *frame_tex = NULL;
static int frame_w = 0;
static int frame_h = 0;
static int tex_w = 0;
static int tex_h = 0;
static SDL_Rect dest_rect = {0};

static void *raw_frame_buf = NULL;
static size_t raw_frame_buf_cap = 0;
static size_t raw_frame_pitch = 0;
static unsigned raw_frame_bpp = 0;

static uint8_t *scale2_x_buf = NULL;
static size_t scale2_x_buf_cap = 0;

static int frame_dirty = 0;

static void upload_frame(void);

static Uint32 sdl_format_for_pixel_format(const enum retro_pixel_format fmt) {
    switch (fmt) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            return SDL_PIXELFORMAT_ARGB8888;
        case RETRO_PIXEL_FORMAT_RGB565:
            return SDL_PIXELFORMAT_RGB565;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
            return SDL_PIXELFORMAT_ARGB1555;
    }
}

static unsigned bpp_for_pixel_format(void) {
    return mux_retro_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
}

static const SDL_Color border_colors[border_color_count] = {
    {0, 0, 0, 255},
    {0, 0, 0, 255},
    {32, 32, 32, 255},
    {255, 255, 255, 255},
};

static void draw_video_background(SDL_Renderer *renderer) {
    if (!frame_tex) return;

    if (session_settings.border_color != border_color_theme) {
        const SDL_Color *c = &border_colors[session_settings.border_color];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, c->a);
        SDL_RenderFillRect(renderer, NULL);
    }

    SDL_RenderCopy(renderer, frame_tex, NULL, &dest_rect);
}

static void recompute_dest_rect(void) {
    if (frame_w == 0 || frame_h == 0) return;

    switch (session_settings.scaling_mode) {
        case video_scale_stretch:
            dest_rect.w = device.mux.width;
            dest_rect.h = device.mux.height;
            break;

        case video_scale_integer_x1:
        case video_scale_integer_x2:
        case video_scale_integer_x3: {
            const int scale = session_settings.scaling_mode - video_scale_integer_x1 + 1;
            dest_rect.w = frame_w * scale;
            dest_rect.h = frame_h * scale;
            break;
        }

        case video_scale_integer_auto: {
            int scale = device.mux.width / frame_w;
            const int scale_h = device.mux.height / frame_h;
            if (scale_h < scale) scale = scale_h;
            if (scale < 1) scale = 1;

            dest_rect.w = frame_w * scale;
            dest_rect.h = frame_h * scale;
            break;
        }

        case video_scale_aspect:
        default: {
            const double src_aspect = (double) frame_w / (double) frame_h;
            const double dst_aspect = (double) device.mux.width / (double) device.mux.height;

            if (src_aspect > dst_aspect) {
                dest_rect.w = device.mux.width;
                dest_rect.h = (int) ((double) device.mux.width / src_aspect);
            } else {
                dest_rect.h = device.mux.height;
                dest_rect.w = (int) ((double) device.mux.height * src_aspect);
            }
            break;
        }
    }

    dest_rect.x = (device.mux.width - dest_rect.w) / 2;
    dest_rect.y = (device.mux.height - dest_rect.h) / 2;
}

void video_bridge_apply_scaling(void) {
    recompute_dest_rect();
}

void video_bridge_apply_fps_limit(void) {
    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer) return;

    const int want_vsync = session_settings.fps_limit == fps_limit_60;

    if (SDL_RenderSetVSync(renderer, want_vsync) != 0) {
        LOG_ERROR(mux_module, "SDL_RenderSetVSync(%d) failed: %s", want_vsync, SDL_GetError());
    } else {
        LOG_INFO(mux_module, "SDL_RenderSetVSync(%d) applied", want_vsync);
    }
}

static void apply_texture_filter(void) {
    if (!frame_tex) return;
    SDL_SetTextureScaleMode(
        frame_tex, session_settings.texture_filter == texture_filter_smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest
    );
}

void video_bridge_apply_filter(void) {
    apply_texture_filter();
    upload_frame();
}

void video_bridge_get_frame_size(int *w, int *h) {
    *w = frame_w;
    *h = frame_h;
}

void video_bridge_get_dest_size(int *w, int *h) {
    *w = dest_rect.w;
    *h = dest_rect.h;
}

#define SCALE2_X_IMPL(NAME, TYPE)                                                                                      \
    static void NAME(                                                                                                  \
        const TYPE *src, TYPE *dst, const int width, const int height, const int src_pitch_px, const int dst_pitch_px  \
    ) {                                                                                                                \
        for (int y = 0; y < height; y++) {                                                                             \
            const TYPE *row = src + (size_t) y * src_pitch_px;                                                         \
            const TYPE *row_up = src + (size_t) (y > 0 ? y - 1 : y) * src_pitch_px;                                    \
            const TYPE *row_down = src + (size_t) (y < height - 1 ? y + 1 : y) * src_pitch_px;                         \
            TYPE *out0 = dst + (size_t) (y * 2) * dst_pitch_px;                                                        \
            TYPE *out1 = dst + (size_t) (y * 2 + 1) * dst_pitch_px;                                                    \
                                                                                                                       \
            for (int x = 0; x < width; x++) {                                                                          \
                const TYPE e = row[x];                                                                                 \
                const TYPE b = row_up[x];                                                                              \
                const TYPE h = row_down[x];                                                                            \
                const TYPE d = row[x > 0 ? x - 1 : x];                                                                 \
                const TYPE f = row[x < width - 1 ? x + 1 : x];                                                         \
                                                                                                                       \
                TYPE e0 = e, e1 = e, e2 = e, e3 = e;                                                                   \
                if (b != h && d != f) {                                                                                \
                    e0 = (d == b) ? d : e;                                                                             \
                    e1 = (b == f) ? f : e;                                                                             \
                    e2 = (d == h) ? d : e;                                                                             \
                    e3 = (h == f) ? f : e;                                                                             \
                }                                                                                                      \
                                                                                                                       \
                out0[x * 2] = e0;                                                                                      \
                out0[x * 2 + 1] = e1;                                                                                  \
                out1[x * 2] = e2;                                                                                      \
                out1[x * 2 + 1] = e3;                                                                                  \
            }                                                                                                          \
        }                                                                                                              \
    }

SCALE2_X_IMPL(scale2x_16, uint16_t)
SCALE2_X_IMPL(scale2x_32, uint32_t)

static void compute_target_tex_size(int *w, int *h) {
    const int scale = session_settings.texture_filter == texture_filter_scale2_x ? 2 : 1;
    *w = frame_w * scale;
    *h = frame_h * scale;
}

static void upload_frame(void) {
    if (!raw_frame_buf || frame_w == 0 || frame_h == 0) return;

    int want_w, want_h;
    compute_target_tex_size(&want_w, &want_h);

    if (!frame_tex || want_w != tex_w || want_h != tex_h) {
        if (frame_tex) SDL_DestroyTexture(frame_tex);

        frame_tex = SDL_CreateTexture(
            display_get_renderer(), sdl_format_for_pixel_format(mux_retro_get_pixel_format()),
            SDL_TEXTUREACCESS_STREAMING, want_w, want_h
        );

        if (!frame_tex) {
            LOG_ERROR(mux_module, "Failed to create video frame texture: %s", SDL_GetError());
            tex_w = 0;
            tex_h = 0;
            return;
        }

        tex_w = want_w;
        tex_h = want_h;
        apply_texture_filter();
    }

    if (session_settings.texture_filter == texture_filter_scale2_x) {
        const size_t needed = (size_t) tex_w * tex_h * raw_frame_bpp;
        if (needed > scale2_x_buf_cap) {
            free(scale2_x_buf);
            scale2_x_buf = malloc(needed);
            scale2_x_buf_cap = scale2_x_buf ? needed : 0;
        }

        if (scale2_x_buf) {
            const int src_pitch_px = (int) (raw_frame_pitch / raw_frame_bpp);

            if (raw_frame_bpp == 4) {
                scale2x_32(raw_frame_buf, (uint32_t *) scale2_x_buf, frame_w, frame_h, src_pitch_px, tex_w);
            } else {
                scale2x_16(raw_frame_buf, (uint16_t *) scale2_x_buf, frame_w, frame_h, src_pitch_px, tex_w);
            }

            SDL_UpdateTexture(frame_tex, NULL, scale2_x_buf, tex_w * (int) raw_frame_bpp);
        }
    } else {
        SDL_UpdateTexture(frame_tex, NULL, raw_frame_buf, (int) raw_frame_pitch);
    }
}

void video_bridge_init(void) {
    display_set_video_background(draw_video_background);
}

void video_bridge_shutdown(void) {
    display_clear_video_background();

    if (frame_tex) {
        SDL_DestroyTexture(frame_tex);
        frame_tex = NULL;
    }

    free(raw_frame_buf);
    raw_frame_buf = NULL;
    raw_frame_buf_cap = 0;

    free(scale2_x_buf);
    scale2_x_buf = NULL;
    scale2_x_buf_cap = 0;

    frame_w = 0;
    frame_h = 0;
    tex_w = 0;
    tex_h = 0;
}

void mux_retro_video_refresh_cb(const void *data, const unsigned width, const unsigned height, const size_t pitch) {
    if (!data || width == 0 || height == 0) return;

    const size_t needed = pitch * height;
    if (needed > raw_frame_buf_cap) {
        void *grown = malloc(needed);
        if (!grown) return;
        free(raw_frame_buf);
        raw_frame_buf = grown;
        raw_frame_buf_cap = needed;
    }

    memcpy(raw_frame_buf, data, needed);
    raw_frame_pitch = pitch;
    raw_frame_bpp = bpp_for_pixel_format();

    const int size_changed = (int) width != frame_w || (int) height != frame_h;
    frame_w = (int) width;
    frame_h = (int) height;

    if (size_changed) recompute_dest_rect();

    frame_dirty = 1;
}

void video_bridge_flush_frame(void) {
    if (!frame_dirty) return;
    frame_dirty = 0;
    upload_frame();
}
