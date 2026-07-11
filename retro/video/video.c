#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/device.h"
#include "../../common/display.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "colour.h"
#include "../core/muxretro.h"
#include "overlay_bridge.h"
#include "../settings/settings.h"

static SDL_Texture *frame_tex = NULL;
static int frame_w = 0;
static int frame_h = 0;
static int tex_w = 0;
static int tex_h = 0;
static Uint32 tex_format = 0;
static SDL_Rect dest_rect = {0};

static double core_aspect_ratio = 0.0;
static int core_rotation_quarters = 0;

static void *raw_frame_buf = NULL;
static size_t raw_frame_buf_cap = 0;
static size_t raw_frame_pitch = 0;
static unsigned raw_frame_bpp = 0;

static uint8_t *scaled_buf = NULL;
static size_t scaled_buf_cap = 0;

static SDL_Texture *sharp_bilinear_tex = NULL;
static int sharp_bilinear_tex_w = 0;
static int sharp_bilinear_tex_h = 0;

static SDL_Texture *rotate_canvas_tex = NULL;
static int rotate_canvas_w = 0;
static int rotate_canvas_h = 0;

static int frame_dirty = 0;
static int frame_skip = 0;

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

static int effective_rotation(void) {
    return ((session_settings.rotate + core_rotation_quarters) % video_rotate_count + video_rotate_count)
           % video_rotate_count;
}

static void get_canvas_size(int *w, int *h) {
    const int rot = effective_rotation();
    if (rot == video_rotate_90 || rot == video_rotate_270) {
        *w = device.mux.height;
        *h = device.mux.width;
    } else {
        *w = device.mux.width;
        *h = device.mux.height;
    }
}

static double compute_src_aspect(void) {
    switch (session_settings.aspect_ratio) {
        case aspect_ratio_4_3:
            return 4.0 / 3.0;
        case aspect_ratio_8_7:
            return 8.0 / 7.0;
        case aspect_ratio_16_9:
            return 16.0 / 9.0;
        case aspect_ratio_16_10:
            return 16.0 / 10.0;
        case aspect_ratio_pixel_perfect:
            return (double) frame_w / (double) frame_h;
        case aspect_ratio_auto:
        default:
            return core_aspect_ratio > 0.0 ? core_aspect_ratio : (double) frame_w / (double) frame_h;
    }
}

static void draw_sharp_bilinear(SDL_Renderer *renderer) {
    int int_scale = frame_w > 0 ? dest_rect.w / frame_w : 1;
    const int int_scale_h = frame_h > 0 ? dest_rect.h / frame_h : 1;
    if (int_scale_h < int_scale) int_scale = int_scale_h;
    if (int_scale < 1) int_scale = 1;

    const int want_w = frame_w * int_scale;
    const int want_h = frame_h * int_scale;

    if (!sharp_bilinear_tex || want_w != sharp_bilinear_tex_w || want_h != sharp_bilinear_tex_h) {
        if (sharp_bilinear_tex) SDL_DestroyTexture(sharp_bilinear_tex);

        sharp_bilinear_tex = SDL_CreateTexture(
            renderer, sdl_format_for_pixel_format(mux_retro_get_pixel_format()), SDL_TEXTUREACCESS_TARGET, want_w,
            want_h
        );

        if (sharp_bilinear_tex) {
            SDL_SetTextureScaleMode(sharp_bilinear_tex, SDL_ScaleModeLinear);
            sharp_bilinear_tex_w = want_w;
            sharp_bilinear_tex_h = want_h;
        } else {
            LOG_ERROR(mux_module, "Failed to create sharp bilinear intermediate texture: %s", SDL_GetError());
            sharp_bilinear_tex_w = 0;
            sharp_bilinear_tex_h = 0;
        }
    }

    if (!sharp_bilinear_tex) {
        colour_render_pass(renderer, frame_tex, &dest_rect);
        return;
    }

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, sharp_bilinear_tex);
    SDL_RenderCopy(renderer, frame_tex, NULL, NULL);
    SDL_SetRenderTarget(renderer, prev_target);

    colour_render_pass(renderer, sharp_bilinear_tex, &dest_rect);
}

static void draw_video_content(SDL_Renderer *renderer) {
    if (session_settings.border_color != border_color_theme) {
        const SDL_Color *c = &border_colors[session_settings.border_color];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, c->a);
        SDL_RenderFillRect(renderer, NULL);
    }

    if (session_settings.texture_filter == texture_filter_sharp_bilinear) {
        draw_sharp_bilinear(renderer);
    } else {
        colour_render_pass(renderer, frame_tex, &dest_rect);
    }

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);
    overlay_bridge_render(renderer, canvas_w, canvas_h);
}

static void draw_video_background(SDL_Renderer *renderer) {
    if (!frame_tex) return;

    const int rot = effective_rotation();

    if (rot == video_rotate_0 && !session_settings.mirrored) {
        draw_video_content(renderer);
        return;
    }

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);

    if (!rotate_canvas_tex || rotate_canvas_w != canvas_w || rotate_canvas_h != canvas_h) {
        if (rotate_canvas_tex) SDL_DestroyTexture(rotate_canvas_tex);

        rotate_canvas_tex =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, canvas_w, canvas_h);

        if (rotate_canvas_tex) {
            SDL_SetTextureBlendMode(rotate_canvas_tex, SDL_BLENDMODE_NONE);
            rotate_canvas_w = canvas_w;
            rotate_canvas_h = canvas_h;
        } else {
            LOG_ERROR(mux_module, "Failed to create rotate canvas texture: %s", SDL_GetError());
            rotate_canvas_w = 0;
            rotate_canvas_h = 0;
        }
    }

    if (!rotate_canvas_tex) {
        draw_video_content(renderer);
        return;
    }

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, rotate_canvas_tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_video_content(renderer);
    SDL_SetRenderTarget(renderer, prev_target);

    const SDL_Rect final_dst = {
        (device.mux.width - canvas_w) / 2, (device.mux.height - canvas_h) / 2, canvas_w, canvas_h
    };

    const SDL_RendererFlip flip = session_settings.mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_RenderCopyEx(renderer, rotate_canvas_tex, NULL, &final_dst, (double) rot * 90.0, NULL, flip);
}

static void recompute_dest_rect(void) {
    if (frame_w == 0 || frame_h == 0) return;

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);

    switch (session_settings.scaling_mode) {
        case video_scale_stretch:
            dest_rect.w = canvas_w;
            dest_rect.h = canvas_h;
            break;

        case video_scale_integer: {
            double scale;
            if (session_settings.integer_scale == integer_scale_auto) {
                int auto_scale = canvas_w / frame_w;
                const int auto_scale_h = canvas_h / frame_h;
                if (auto_scale_h < auto_scale) auto_scale = auto_scale_h;
                if (auto_scale < 1) auto_scale = 1;
                scale = (double) auto_scale;
            } else {
                scale = session_settings_integer_scale_value(session_settings.integer_scale);
            }

            dest_rect.w = (int) ((double) frame_w * scale);
            dest_rect.h = (int) ((double) frame_h * scale);
            break;
        }

        case video_scale_full_height: {
            const double src_aspect = compute_src_aspect();
            dest_rect.h = canvas_h;
            dest_rect.w = (int) ((double) dest_rect.h * src_aspect);
            break;
        }

        case video_scale_full_width: {
            const double src_aspect = compute_src_aspect();
            dest_rect.w = canvas_w;
            dest_rect.h = (int) ((double) dest_rect.w / src_aspect);
            break;
        }

        case video_scale_aspect:
        default: {
            const double src_aspect = compute_src_aspect();

            double height_scale;
            if (session_settings.integer_scale == integer_scale_auto) {
                int scale = canvas_h / frame_h;
                if (scale < 1) scale = 1;
                while (scale > 1 && (double) frame_h * scale * src_aspect > (double) canvas_w)
                    scale--;
                height_scale = (double) scale;
            } else {
                height_scale = session_settings_integer_scale_value(session_settings.integer_scale);
            }

            dest_rect.h = (int) ((double) frame_h * height_scale);
            dest_rect.w = (int) ((double) dest_rect.h * src_aspect);
            break;
        }
    }

    if (session_settings.viewport_zoom != 100) {
        dest_rect.w = dest_rect.w * session_settings.viewport_zoom / 100;
        dest_rect.h = dest_rect.h * session_settings.viewport_zoom / 100;
    }

    if (session_settings.shimmer_fix) {
        int width_scale = (dest_rect.w + frame_w / 2) / frame_w;
        if (width_scale < 1) width_scale = 1;
        int height_scale = (dest_rect.h + frame_h / 2) / frame_h;
        if (height_scale < 1) height_scale = 1;

        dest_rect.w = frame_w * width_scale;
        dest_rect.h = frame_h * height_scale;
    }

    dest_rect.x = (canvas_w - dest_rect.w) / 2 + session_settings.viewport_offset_x;
    dest_rect.y = (canvas_h - dest_rect.h) / 2 + session_settings.viewport_offset_y;
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
    static inline void NAME##_px(                                                                                      \
        const TYPE b, const TYPE d, const TYPE e, const TYPE f, const TYPE h, TYPE *out0, TYPE *out1                   \
    ) {                                                                                                                \
        TYPE e0 = e, e1 = e, e2 = e, e3 = e;                                                                           \
        if (b != h && d != f) {                                                                                        \
            e0 = (d == b) ? d : e;                                                                                     \
            e1 = (b == f) ? f : e;                                                                                     \
            e2 = (d == h) ? d : e;                                                                                     \
            e3 = (h == f) ? f : e;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        out0[0] = e0;                                                                                                  \
        out0[1] = e1;                                                                                                  \
        out1[0] = e2;                                                                                                  \
        out1[1] = e3;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
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
            NAME##_px(row_up[0], row[0], row[0], row[width > 1 ? 1 : 0], row_down[0], out0, out1);                     \
                                                                                                                       \
            for (int x = 1; x + 1 < width; x++)                                                                        \
                NAME##_px(row_up[x], row[x - 1], row[x], row[x + 1], row_down[x], out0 + x * 2, out1 + x * 2);         \
                                                                                                                       \
            if (width > 1) {                                                                                           \
                const int x = width - 1;                                                                               \
                NAME##_px(row_up[x], row[x - 1], row[x], row[x], row_down[x], out0 + x * 2, out1 + x * 2);             \
            }                                                                                                          \
        }                                                                                                              \
    }

SCALE2_X_IMPL(scale2x_16, uint16_t)
SCALE2_X_IMPL(scale2x_32, uint32_t)

#define SCALE3_X_IMPL(NAME, TYPE)                                                                                      \
    static inline void NAME##_px(                                                                                      \
        const TYPE a, const TYPE b, const TYPE c, const TYPE d, const TYPE e, const TYPE f, const TYPE g,              \
        const TYPE h, const TYPE ii, TYPE *out0, TYPE *out1, TYPE *out2                                                \
    ) {                                                                                                                \
        TYPE e0 = e, e1 = e, e2 = e, e3 = e, e5 = e, e6 = e, e7 = e, e8 = e;                                           \
        if (b != h && d != f) {                                                                                        \
            e0 = (d == b) ? d : e;                                                                                     \
            e1 = ((d == b && e != c) || (b == f && e != a)) ? b : e;                                                   \
            e2 = (b == f) ? f : e;                                                                                     \
            e3 = ((d == b && e != g) || (d == h && e != a)) ? d : e;                                                   \
            e5 = ((b == f && e != ii) || (h == f && e != c)) ? f : e;                                                  \
            e6 = (d == h) ? d : e;                                                                                     \
            e7 = ((d == h && e != ii) || (h == f && e != g)) ? h : e;                                                  \
            e8 = (h == f) ? f : e;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        out0[0] = e0;                                                                                                  \
        out0[1] = e1;                                                                                                  \
        out0[2] = e2;                                                                                                  \
        out1[0] = e3;                                                                                                  \
        out1[1] = e;                                                                                                   \
        out1[2] = e5;                                                                                                  \
        out2[0] = e6;                                                                                                  \
        out2[1] = e7;                                                                                                  \
        out2[2] = e8;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    static void NAME(                                                                                                  \
        const TYPE *src, TYPE *dst, const int width, const int height, const int src_pitch_px, const int dst_pitch_px  \
    ) {                                                                                                                \
        for (int y = 0; y < height; y++) {                                                                             \
            const TYPE *row = src + (size_t) y * src_pitch_px;                                                         \
            const TYPE *row_up = src + (size_t) (y > 0 ? y - 1 : y) * src_pitch_px;                                    \
            const TYPE *row_down = src + (size_t) (y < height - 1 ? y + 1 : y) * src_pitch_px;                         \
            TYPE *out0 = dst + (size_t) (y * 3) * dst_pitch_px;                                                        \
            TYPE *out1 = dst + (size_t) (y * 3 + 1) * dst_pitch_px;                                                    \
            TYPE *out2 = dst + (size_t) (y * 3 + 2) * dst_pitch_px;                                                    \
                                                                                                                       \
            {                                                                                                          \
                const int xr = width > 1 ? 1 : 0;                                                                      \
                NAME##_px(                                                                                             \
                    row_up[0], row_up[0], row_up[xr], row[0], row[0], row[xr], row_down[0], row_down[0], row_down[xr], \
                    out0, out1, out2                                                                                   \
                );                                                                                                     \
            }                                                                                                          \
                                                                                                                       \
            for (int x = 1; x + 1 < width; x++)                                                                        \
                NAME##_px(                                                                                             \
                    row_up[x - 1], row_up[x], row_up[x + 1], row[x - 1], row[x], row[x + 1], row_down[x - 1],          \
                    row_down[x], row_down[x + 1], out0 + x * 3, out1 + x * 3, out2 + x * 3                             \
                );                                                                                                     \
                                                                                                                       \
            if (width > 1) {                                                                                           \
                const int x = width - 1;                                                                               \
                NAME##_px(                                                                                             \
                    row_up[x - 1], row_up[x], row_up[x], row[x - 1], row[x], row[x], row_down[x - 1], row_down[x],     \
                    row_down[x], out0 + x * 3, out1 + x * 3, out2 + x * 3                                              \
                );                                                                                                     \
            }                                                                                                          \
        }                                                                                                              \
    }

SCALE3_X_IMPL(scale3x_16, uint16_t)
SCALE3_X_IMPL(scale3x_32, uint32_t)

static void compute_target_tex_size(int *w, int *h) {
    int scale = 1;
    if (session_settings.texture_filter == texture_filter_scale2_x) {
        scale = 2;
    } else if (session_settings.texture_filter == texture_filter_scale3_x) {
        scale = 3;
    }
    *w = frame_w * scale;
    *h = frame_h * scale;
}

static int ensure_frame_tex(const int want_w, const int want_h, const Uint32 want_format) {
    if (frame_tex && want_w == tex_w && want_h == tex_h && want_format == tex_format) return 1;

    if (frame_tex) SDL_DestroyTexture(frame_tex);

    frame_tex = SDL_CreateTexture(display_get_renderer(), want_format, SDL_TEXTUREACCESS_STREAMING, want_w, want_h);

    if (!frame_tex) {
        LOG_ERROR(mux_module, "Failed to create video frame texture: %s", SDL_GetError());
        tex_w = 0;
        tex_h = 0;
        tex_format = 0;
        return 0;
    }

    tex_w = want_w;
    tex_h = want_h;
    tex_format = want_format;
    apply_texture_filter();
    return 1;
}

static void upload_frame(void) {
    if (!raw_frame_buf || frame_w == 0 || frame_h == 0) return;

    int want_w, want_h;
    compute_target_tex_size(&want_w, &want_h);

    if (!ensure_frame_tex(want_w, want_h, sdl_format_for_pixel_format(mux_retro_get_pixel_format()))) return;

    const size_t needed = (size_t) want_w * want_h * raw_frame_bpp;
    if (needed > scaled_buf_cap) {
        free(scaled_buf);
        scaled_buf = malloc(needed);
        scaled_buf_cap = scaled_buf ? needed : 0;
    }

    if (!scaled_buf) return;

    const int src_pitch_px = (int) (raw_frame_pitch / raw_frame_bpp);
    const int is_scale3_x = session_settings.texture_filter == texture_filter_scale3_x;

    if (raw_frame_bpp == 4) {
        if (is_scale3_x) {
            scale3x_32(raw_frame_buf, (uint32_t *) scaled_buf, frame_w, frame_h, src_pitch_px, want_w);
        } else {
            scale2x_32(raw_frame_buf, (uint32_t *) scaled_buf, frame_w, frame_h, src_pitch_px, want_w);
        }
    } else {
        if (is_scale3_x) {
            scale3x_16(raw_frame_buf, (uint16_t *) scaled_buf, frame_w, frame_h, src_pitch_px, want_w);
        } else {
            scale2x_16(raw_frame_buf, (uint16_t *) scaled_buf, frame_w, frame_h, src_pitch_px, want_w);
        }
    }

    SDL_UpdateTexture(frame_tex, NULL, scaled_buf, want_w * (int) raw_frame_bpp);
}

void video_bridge_init(void) {
    display_set_video_background(draw_video_background);
}

void video_bridge_set_core_aspect(const double aspect_ratio) {
    core_aspect_ratio = aspect_ratio;
    recompute_dest_rect();
}

void video_bridge_set_geometry(const unsigned base_width, const unsigned base_height, const float aspect_ratio) {
    if (aspect_ratio > 0.0f) {
        core_aspect_ratio = (double) aspect_ratio;
    } else if (base_width > 0 && base_height > 0) {
        core_aspect_ratio = (double) base_width / (double) base_height;
    }

    recompute_dest_rect();
}

void video_bridge_set_core_rotation(const int quarter_turns) {
    core_rotation_quarters = ((quarter_turns % video_rotate_count) + video_rotate_count) % video_rotate_count;
    recompute_dest_rect();
}

void video_bridge_shutdown(void) {
    display_clear_video_background();

    if (frame_tex) {
        SDL_DestroyTexture(frame_tex);
        frame_tex = NULL;
    }

    if (sharp_bilinear_tex) {
        SDL_DestroyTexture(sharp_bilinear_tex);
        sharp_bilinear_tex = NULL;
    }

    sharp_bilinear_tex_w = 0;
    sharp_bilinear_tex_h = 0;

    if (rotate_canvas_tex) {
        SDL_DestroyTexture(rotate_canvas_tex);
        rotate_canvas_tex = NULL;
    }

    rotate_canvas_w = 0;
    rotate_canvas_h = 0;

    free(raw_frame_buf);
    raw_frame_buf = NULL;
    raw_frame_buf_cap = 0;

    free(scaled_buf);
    scaled_buf = NULL;
    scaled_buf_cap = 0;

    frame_w = 0;
    frame_h = 0;
    tex_w = 0;
    tex_h = 0;
    tex_format = 0;
}

void video_bridge_set_frame_skip(const int skip) {
    frame_skip = skip;
}

int video_bridge_get_frame_skip(void) {
    return frame_skip;
}

void mux_retro_video_refresh_cb(const void *data, const unsigned width, const unsigned height, const size_t pitch) {
    if (frame_skip || !data || width == 0 || height == 0) return;

    raw_frame_pitch = pitch;
    raw_frame_bpp = bpp_for_pixel_format();

    const int size_changed = (int) width != frame_w || (int) height != frame_h;
    frame_w = (int) width;
    frame_h = (int) height;

    if (size_changed) recompute_dest_rect();

    const int cpu_filter = session_settings.texture_filter == texture_filter_scale2_x
                           || session_settings.texture_filter == texture_filter_scale3_x;

    if (cpu_filter) {
        const size_t needed = pitch * height;
        if (needed > raw_frame_buf_cap) {
            void *grown = malloc(needed);
            if (!grown) return;
            free(raw_frame_buf);
            raw_frame_buf = grown;
            raw_frame_buf_cap = needed;
        }

        memcpy(raw_frame_buf, data, needed);
        frame_dirty = 1;
        return;
    }

    if (ensure_frame_tex(frame_w, frame_h, sdl_format_for_pixel_format(mux_retro_get_pixel_format())))
        SDL_UpdateTexture(frame_tex, NULL, data, (int) pitch);
}

void video_bridge_flush_frame(void) {
    if (!frame_dirty) return;
    frame_dirty = 0;
    upload_frame();
}
