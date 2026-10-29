#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <common/platform/device.h>
#include <common/platform/display.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/ui/crt.h>
#include "colour.h"
#include "geometry.h"
#include "filters/filters.h"
#include "hw_render.h"
#include "interframe_blend.h"
#include "../core/muxretro.h"
#include "../core/perf.h"
#include "overlay_bridge.h"
#include "../link/link.h"
#include "../settings/settings.h"

static SDL_Texture *frame_tex = NULL;
static int frame_w = 0;
static int frame_h = 0;
static int tex_w = 0;
static int tex_h = 0;
static Uint32 tex_format = 0;
static SDL_Rect dest_rect = {0};
static SDL_Rect crop_src_rect = {0};
static SDL_Rect last_output_rect = {0};
static int crop_active = 0;

static double core_aspect_ratio = 0.0;
static int core_rotation_quarters = 0;

static void *raw_frame_buf = NULL;
static size_t raw_frame_buf_cap = 0;
static size_t raw_frame_pitch = 0;
static unsigned raw_frame_bpp = 0;
static int raw_frame_valid = 0;
static uint64_t direct_upload_frames = 0;
static uint64_t staged_upload_frames = 0;
static uint64_t staged_upload_bytes = 0;

static uint8_t *scaled_buf = NULL;
static size_t scaled_buf_cap = 0;

// This may need to be increased or perhaps set by device later on...
enum { anti_flicker_history_frames = 4 };

static void *anti_flicker_history[anti_flicker_history_frames] = {NULL};
static size_t anti_flicker_history_cap[anti_flicker_history_frames] = {0};
static uint8_t *anti_flicker_persistence = NULL;
static size_t anti_flicker_persistence_cap = 0;
static size_t anti_flicker_history_pitch = 0;
static int anti_flicker_history_w = 0;
static int anti_flicker_history_h = 0;
static enum retro_pixel_format anti_flicker_history_format = RETRO_PIXEL_FORMAT_0RGB1555;
static unsigned anti_flicker_history_count = 0;
static int anti_flicker_available = 1;
static size_t anti_flicker_allocated_bytes = 0;

static SDL_Texture *sharp_bilinear_tex = NULL;
static int sharp_bilinear_tex_w = 0;
static int sharp_bilinear_tex_h = 0;

static SDL_Texture *rotate_canvas_tex = NULL;
static int rotate_canvas_w = 0;
static int rotate_canvas_h = 0;

static SDL_Texture *output_canvas_tex = NULL;
static int output_canvas_w = 0;
static int output_canvas_h = 0;

static SDL_Texture *crt_snapshot_tex = NULL;
static uint32_t crt_power_off_start = 0;
static uint32_t crt_power_on_start = 0;
static uint32_t crt_degauss_start = 0;

static int frame_dirty = 0;
static int frame_skip = 0;
static int applied_swap_interval = -2;
static int vsync_in_effect = 0;
static int cpu_filter_active = 0;
static int cpu_filter_limit_logged = 0;

enum { max_output_pixels = 1920 * 1080, max_output_dimension = 1920, anti_flicker_max_bytes = 64 * 1024 * 1024 };

static void upload_frame(void);
static void draw_crt_snapshot(SDL_Renderer *renderer, const SDL_Rect *rect, void *userdata);

static void reset_anti_flicker_history(const int release) {
    anti_flicker_history_count = 0;
    anti_flicker_history_pitch = 0;
    anti_flicker_history_w = 0;
    anti_flicker_history_h = 0;
    if (anti_flicker_persistence) memset(anti_flicker_persistence, 0, anti_flicker_persistence_cap);

    if (!release) return;

    for (int i = 0; i < anti_flicker_history_frames; i++) {
        free(anti_flicker_history[i]);
        anti_flicker_history[i] = NULL;
        anti_flicker_history_cap[i] = 0;
    }
    free(anti_flicker_persistence);
    anti_flicker_persistence = NULL;
    anti_flicker_persistence_cap = 0;
    anti_flicker_allocated_bytes = 0;
    interframe_blend_shutdown();
}

void video_bridge_reset_temporal(void) {
    reset_anti_flicker_history(0);
}

void video_bridge_apply_anti_flicker(void) {
    anti_flicker_available = 1;
    reset_anti_flicker_history(!session_settings.anti_flicker);
}

static int ensure_anti_flicker_history(const size_t needed, const size_t persistence_needed) {
    if (persistence_needed > anti_flicker_max_bytes || needed > SIZE_MAX / anti_flicker_history_frames
        || needed * anti_flicker_history_frames > anti_flicker_max_bytes - persistence_needed) {
        LOG_WARN(mux_module, "Anti-Flicker disabled: frame history exceeds the 64 MiB production budget");
        reset_anti_flicker_history(1);
        anti_flicker_available = 0;
        return 0;
    }
    for (int i = 0; i < anti_flicker_history_frames; i++) {
        if (anti_flicker_history_cap[i] >= needed) continue;

        void *grown = realloc(anti_flicker_history[i], needed);
        if (!grown) {
            LOG_ERROR(mux_module, "Failed to allocate Anti-Flicker frame history");
            reset_anti_flicker_history(1);
            anti_flicker_available = 0;
            return 0;
        }

        anti_flicker_history[i] = grown;
        anti_flicker_history_cap[i] = needed;
    }

    if (anti_flicker_persistence_cap < persistence_needed) {
        uint8_t *grown = realloc(anti_flicker_persistence, persistence_needed);
        if (!grown) {
            LOG_ERROR(mux_module, "Failed to allocate Anti-Flicker persistence mask");
            reset_anti_flicker_history(1);
            anti_flicker_available = 0;
            return 0;
        }
        anti_flicker_persistence = grown;
        anti_flicker_persistence_cap = persistence_needed;
        memset(anti_flicker_persistence, 0, persistence_needed);
    }

    anti_flicker_allocated_bytes = needed * anti_flicker_history_frames + persistence_needed;

    return 1;
}

size_t video_bridge_anti_flicker_bytes(void) {
    return anti_flicker_allocated_bytes;
}

int video_bridge_anti_flicker_available(void) {
    return anti_flicker_available && !hw_render_bridge_active();
}

int video_bridge_cpu_filter_active(void) {
    return cpu_filter_active;
}

uint64_t video_bridge_direct_upload_frames(void) {
    return direct_upload_frames;
}

uint64_t video_bridge_staged_upload_frames(void) {
    return staged_upload_frames;
}

uint64_t video_bridge_staged_upload_bytes(void) {
    return staged_upload_bytes;
}

static void apply_anti_flicker(
    const void *original, const unsigned width, const unsigned height, const size_t pitch,
    const enum retro_pixel_format format
) {
    if (!session_settings.anti_flicker || !anti_flicker_available) return;
    if (!original || !width || !height || !pitch) return;

    const size_t bytes_per_pixel = format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    if (width > SIZE_MAX / bytes_per_pixel || pitch < (size_t) width * bytes_per_pixel || pitch % bytes_per_pixel != 0
        || height > SIZE_MAX / pitch || height > SIZE_MAX / width)
        return;

    if (anti_flicker_history_w != (int) width || anti_flicker_history_h != (int) height
        || anti_flicker_history_pitch != pitch || anti_flicker_history_format != format) {
        reset_anti_flicker_history(0);
        anti_flicker_history_w = (int) width;
        anti_flicker_history_h = (int) height;
        anti_flicker_history_pitch = pitch;
        anti_flicker_history_format = format;
    }

    const size_t needed = pitch * height;
    const size_t persistence_needed = (size_t) width * height;
    if (!ensure_anti_flicker_history(needed, persistence_needed)) return;

    if (anti_flicker_history_count >= 2) {
        const uint64_t filter_start = perf_begin();
        interframe_blend_detected(
            raw_frame_buf, anti_flicker_history[0], anti_flicker_history[1],
            anti_flicker_history_count >= 4 ? anti_flicker_history[2] : NULL,
            anti_flicker_history_count >= 4 ? anti_flicker_history[3] : NULL, width, height, pitch, format,
            anti_flicker_persistence, width
        );
        perf_end(perf_stage_anti_flicker, filter_start);
    }

    void *oldest = anti_flicker_history[anti_flicker_history_frames - 1];
    const size_t oldest_cap = anti_flicker_history_cap[anti_flicker_history_frames - 1];
    for (int i = anti_flicker_history_frames - 1; i > 0; i--) {
        anti_flicker_history[i] = anti_flicker_history[i - 1];
        anti_flicker_history_cap[i] = anti_flicker_history_cap[i - 1];
    }
    anti_flicker_history[0] = oldest;
    anti_flicker_history_cap[0] = oldest_cap;

    memcpy(anti_flicker_history[0], original, needed);
    if (anti_flicker_history_count < anti_flicker_history_frames) anti_flicker_history_count++;
}

static Uint32 sdl_format_for_pixel_format(const enum retro_pixel_format fmt) {
    switch (fmt) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            return SDL_PIXELFORMAT_XRGB8888;
        case RETRO_PIXEL_FORMAT_RGB565:
            return SDL_PIXELFORMAT_RGB565;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
            return SDL_PIXELFORMAT_XRGB1555;
    }
}

static unsigned bpp_for_pixel_format(void) {
    return mux_retro_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
}

static const SDL_Color border_colours[border_colour_count] = {
    {0, 0, 0, 255},
    {0, 0, 0, 255},
    {32, 32, 32, 255},
    {255, 255, 255, 255},
};

static int split_frame_w(void) {
    return link_single_screen() && link_split_is_horizontal() ? frame_w / 2 : frame_w;
}

static int split_frame_h(void) {
    return link_single_screen() && !link_split_is_horizontal() ? frame_h / 2 : frame_h;
}

static int clamp_crop(int value, const int max) {
    if (value < 0) value = 0;
    if (value > max) value = max;
    return value;
}

static void apply_viewport_crop(const int canvas_w, const int canvas_h) {
    crop_active = 0;

    crop_src_rect.x = 0;
    crop_src_rect.y = 0;
    crop_src_rect.w = frame_w;
    crop_src_rect.h = frame_h;

    if (link_single_screen()) {
        crop_active = 1;

        crop_src_rect.w = split_frame_w();
        crop_src_rect.h = split_frame_h();
        crop_src_rect.x = link_split_is_horizontal() ? crop_src_rect.w * link_get_focus() : 0;
        crop_src_rect.y = link_split_is_horizontal() ? 0 : crop_src_rect.h * link_get_focus();

        dest_rect.x = (canvas_w - dest_rect.w) / 2;
        dest_rect.y = (canvas_h - dest_rect.h) / 2;
        return;
    }

    const int crop_left = clamp_crop(session_settings.viewport_crop_left, frame_w - 1);
    const int crop_right = clamp_crop(session_settings.viewport_crop_right, frame_w - 1 - crop_left);
    const int crop_top = clamp_crop(session_settings.viewport_crop_top, frame_h - 1);
    const int crop_bottom = clamp_crop(session_settings.viewport_crop_bottom, frame_h - 1 - crop_top);

    if (crop_left || crop_right || crop_top || crop_bottom) {
        crop_active = 1;

        crop_src_rect.x = crop_left;
        crop_src_rect.y = crop_top;
        crop_src_rect.w = frame_w - crop_left - crop_right;
        crop_src_rect.h = frame_h - crop_top - crop_bottom;

        const double scale_x = (double) dest_rect.w / (double) frame_w;
        const double scale_y = (double) dest_rect.h / (double) frame_h;

        const int left_px = (int) ((double) crop_left * scale_x + 0.5);
        const int right_px = (int) ((double) crop_right * scale_x + 0.5);
        const int top_px = (int) ((double) crop_top * scale_y + 0.5);
        const int bottom_px = (int) ((double) crop_bottom * scale_y + 0.5);

        dest_rect.x += left_px;
        dest_rect.y += top_px;
        dest_rect.w -= left_px + right_px;
        dest_rect.h -= top_px + bottom_px;

        if (dest_rect.w < 1) dest_rect.w = 1;
        if (dest_rect.h < 1) dest_rect.h = 1;
    }

    if (session_settings.viewport_centre_crop) {
        dest_rect.x = (canvas_w - dest_rect.w) / 2;
        dest_rect.y = (canvas_h - dest_rect.h) / 2;
    }
}

static const SDL_Rect *crop_tex_rect(SDL_Rect *out, const int scale) {
    if (!crop_active || scale < 1) return NULL;

    out->x = crop_src_rect.x * scale;
    out->y = crop_src_rect.y * scale;
    out->w = crop_src_rect.w * scale;
    out->h = crop_src_rect.h * scale;
    return out;
}

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
            return (double) split_frame_w() / (double) split_frame_h();
        case aspect_ratio_auto:
        default:
            if (link_single_screen()) return (double) split_frame_w() / (double) split_frame_h();
            return core_aspect_ratio > 0.0 ? core_aspect_ratio : (double) frame_w / (double) frame_h;
    }
}

static void draw_sharp_bilinear(SDL_Renderer *renderer, const SDL_Rect *output_rect) {
    const int vis_w = crop_active ? crop_src_rect.w : frame_w;
    const int vis_h = crop_active ? crop_src_rect.h : frame_h;

    int int_scale = vis_w > 0 ? output_rect->w / vis_w : 1;
    const int int_scale_h = vis_h > 0 ? output_rect->h / vis_h : 1;
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

    SDL_Rect crop_src;

    if (!sharp_bilinear_tex) {
        colour_render_pass(renderer, frame_tex, crop_tex_rect(&crop_src, 1), output_rect);
        return;
    }

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, sharp_bilinear_tex);
    SDL_RenderCopy(renderer, frame_tex, NULL, NULL);
    SDL_SetRenderTarget(renderer, prev_target);

    colour_render_pass(renderer, sharp_bilinear_tex, crop_tex_rect(&crop_src, int_scale), output_rect);
}

#define ATMOSPHERE_SEGMENTS   8
#define ATMOSPHERE_SMOOTH_MS  180.0f
#define ATMOSPHERE_INNER_GAIN 0.85f
#define ATMOSPHERE_OUTER_GAIN 0.25f

typedef struct {
    float r;
    float g;
    float b;
} atmosphere_colour;

static atmosphere_colour atmosphere_target[4][ATMOSPHERE_SEGMENTS];
static atmosphere_colour atmosphere_current[4][ATMOSPHERE_SEGMENTS];
static int atmosphere_ready;
static uint32_t atmosphere_tick;

static atmosphere_colour atmosphere_pixel(const uint8_t *row, const int x, const enum retro_pixel_format format) {
    if (format == RETRO_PIXEL_FORMAT_XRGB8888) {
        uint32_t pixel;
        memcpy(&pixel, row + (size_t) x * 4, sizeof(pixel));
        return (atmosphere_colour) {(float) ((pixel >> 16) & 0xFF), (float) ((pixel >> 8) & 0xFF),
                                    (float) (pixel & 0xFF)};
    }

    uint16_t pixel;
    memcpy(&pixel, row + (size_t) x * 2, sizeof(pixel));

    if (format == RETRO_PIXEL_FORMAT_RGB565) {
        return (atmosphere_colour) {(float) ((pixel >> 11) & 0x1F) * (255.0f / 31.0f),
                                    (float) ((pixel >> 5) & 0x3F) * (255.0f / 63.0f),
                                    (float) (pixel & 0x1F) * (255.0f / 31.0f)};
    }

    return (atmosphere_colour) {(float) ((pixel >> 10) & 0x1F) * (255.0f / 31.0f),
                                (float) ((pixel >> 5) & 0x1F) * (255.0f / 31.0f),
                                (float) (pixel & 0x1F) * (255.0f / 31.0f)};
}

static atmosphere_colour atmosphere_cell(
    const uint8_t *data, const size_t pitch, const enum retro_pixel_format format, const int x0, const int y0,
    const int x1, const int y1
) {
    atmosphere_colour sum = {0.0f, 0.0f, 0.0f};
    const int step_x = (x1 - x0) / 8 > 1 ? (x1 - x0) / 8 : 1;
    const int step_y = (y1 - y0) / 8 > 1 ? (y1 - y0) / 8 : 1;
    int count = 0;

    for (int y = y0; y < y1; y += step_y) {
        const uint8_t *row = data + (size_t) y * pitch;
        for (int x = x0; x < x1; x += step_x) {
            const atmosphere_colour colour = atmosphere_pixel(row, x, format);
            sum.r += colour.r;
            sum.g += colour.g;
            sum.b += colour.b;
            count++;
        }
    }

    if (!count) return sum;
    return (atmosphere_colour) {sum.r / (float) count, sum.g / (float) count, sum.b / (float) count};
}

static int clamp_axis(const int value, const int minimum, const int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void
atmosphere_vertex(SDL_Vertex *vertex, const float x, const float y, const atmosphere_colour colour, const float gain) {
    const float r = colour.r * gain > 255.0f ? 255.0f : colour.r * gain;
    const float g = colour.g * gain > 255.0f ? 255.0f : colour.g * gain;
    const float b = colour.b * gain > 255.0f ? 255.0f : colour.b * gain;
    vertex->position.x = x;
    vertex->position.y = y;
    vertex->color = (SDL_Color) {(Uint8) r, (Uint8) g, (Uint8) b, 255};
    vertex->tex_coord.x = 0.0f;
    vertex->tex_coord.y = 0.0f;
}

static void draw_atmosphere_side(
    SDL_Renderer *renderer, const atmosphere_colour *colours, const float start_x, const float start_y,
    const float end_x, const float end_y, const float outer_dx, const float outer_dy
) {
    SDL_Vertex vertices[(ATMOSPHERE_SEGMENTS + 1) * 2];
    int indices[ATMOSPHERE_SEGMENTS * 6];

    for (int point = 0; point <= ATMOSPHERE_SEGMENTS; point++) {
        atmosphere_colour colour;
        if (point == 0) {
            colour = colours[0];
        } else if (point == ATMOSPHERE_SEGMENTS) {
            colour = colours[ATMOSPHERE_SEGMENTS - 1];
        } else {
            colour = (atmosphere_colour) {(colours[point - 1].r + colours[point].r) * 0.5f,
                                          (colours[point - 1].g + colours[point].g) * 0.5f,
                                          (colours[point - 1].b + colours[point].b) * 0.5f};
        }

        const float share = (float) point / ATMOSPHERE_SEGMENTS;
        const float x = start_x + (end_x - start_x) * share;
        const float y = start_y + (end_y - start_y) * share;
        atmosphere_vertex(&vertices[point * 2], x, y, colour, ATMOSPHERE_INNER_GAIN);
        atmosphere_vertex(&vertices[point * 2 + 1], x + outer_dx, y + outer_dy, colour, ATMOSPHERE_OUTER_GAIN);
    }

    for (int segment = 0; segment < ATMOSPHERE_SEGMENTS; segment++) {
        const int base = segment * 2;
        int *index = &indices[segment * 6];
        index[0] = base;
        index[1] = base + 1;
        index[2] = base + 2;
        index[3] = base + 1;
        index[4] = base + 3;
        index[5] = base + 2;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, (ATMOSPHERE_SEGMENTS + 1) * 2, indices, ATMOSPHERE_SEGMENTS * 6);
}

static void draw_atmosphere_border(SDL_Renderer *renderer, const SDL_Rect *output_rect, const int physical_output) {
    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);

    SDL_Rect canvas = {0, 0, canvas_w, canvas_h};
    if (physical_output) {
        const SDL_Rect logical_canvas = canvas;
        display_map_logical_rect(&logical_canvas, &canvas);
    }

    SDL_SetRenderDrawColor(renderer, 4, 4, 4, 255);
    SDL_RenderFillRect(renderer, &canvas);

    if (!atmosphere_ready) return;

    const uint32_t now = SDL_GetTicks();
    float blend = atmosphere_tick ? (float) (now - atmosphere_tick) / ATMOSPHERE_SMOOTH_MS : 1.0f;
    if (blend > 1.0f) blend = 1.0f;
    atmosphere_tick = now;

    for (int side = 0; side < 4; side++) {
        for (int segment = 0; segment < ATMOSPHERE_SEGMENTS; segment++) {
            atmosphere_colour *current = &atmosphere_current[side][segment];
            const atmosphere_colour *wanted = &atmosphere_target[side][segment];
            current->r += (wanted->r - current->r) * blend;
            current->g += (wanted->g - current->g) * blend;
            current->b += (wanted->b - current->b) * blend;
        }
    }

    const int canvas_right = canvas.x + canvas.w;
    const int canvas_bottom = canvas.y + canvas.h;
    const float left = (float) clamp_axis(output_rect->x, canvas.x, canvas_right);
    const float right = (float) clamp_axis(output_rect->x + output_rect->w, canvas.x, canvas_right);
    const float top = (float) clamp_axis(output_rect->y, canvas.y, canvas_bottom);
    const float bottom = (float) clamp_axis(output_rect->y + output_rect->h, canvas.y, canvas_bottom);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    if (left > (float) canvas.x)
        draw_atmosphere_side(renderer, atmosphere_current[0], left, top, left, bottom, (float) canvas.x - left, 0.0f);
    if (right < (float) canvas_right)
        draw_atmosphere_side(
            renderer, atmosphere_current[1], right, top, right, bottom, (float) canvas_right - right, 0.0f
        );
    if (top > (float) canvas.y)
        draw_atmosphere_side(renderer, atmosphere_current[2], left, top, right, top, 0.0f, (float) canvas.y - top);
    if (bottom < (float) canvas_bottom)
        draw_atmosphere_side(
            renderer, atmosphere_current[3], left, bottom, right, bottom, 0.0f, (float) canvas_bottom - bottom
        );
}

static void draw_video_content(SDL_Renderer *renderer, const int physical_output) {
    SDL_Rect output_rect = dest_rect;
    if (physical_output) display_map_logical_rect(&dest_rect, &output_rect);

    const int source_w = crop_active ? crop_src_rect.w : split_frame_w();
    const int source_h = crop_active ? crop_src_rect.h : split_frame_h();
    if (session_settings.shimmer_fix) video_geometry_snap_integer(&output_rect, source_w, source_h);
    last_output_rect = output_rect;

    const int border_index = session_settings.border_colour >= 0 && session_settings.border_colour < border_colour_count
                                 ? session_settings.border_colour
                                 : border_colour_atmosphere;
    if (border_index == border_colour_atmosphere) {
        draw_atmosphere_border(renderer, &output_rect, physical_output);
    } else {
        const SDL_Color *c = &border_colours[border_index];
        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, c->a);
        SDL_RenderFillRect(renderer, NULL);
    }

    if (hw_render_bridge_active()) {
        hw_render_bridge_draw(renderer, &output_rect, crop_active ? &crop_src_rect : NULL);
    } else if (session_settings.texture_filter == texture_filter_sharp_bilinear) {
        draw_sharp_bilinear(renderer, &output_rect);
    } else {
        SDL_Rect crop_src;
        const SDL_Rect *source = crop_tex_rect(&crop_src, frame_w > 0 ? tex_w / frame_w : 1);
        if (session_settings.texture_filter == texture_filter_nearest)
            colour_render_pass_area_scaled(renderer, frame_tex, source, &output_rect);
        else
            colour_render_pass(renderer, frame_tex, source, &output_rect);
    }

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);
    overlay_bridge_render(renderer, canvas_w, canvas_h, physical_output);
}

static void draw_video_background_logical(SDL_Renderer *renderer, const int physical_output) {
    if ((crt_power_off_start || crt_power_on_start || crt_degauss_start) && crt_snapshot_tex) {
        SDL_Rect frame = {0, 0, device.mux.width, device.mux.height};
        if (physical_output) {
            const SDL_Rect logical_frame = frame;
            display_map_logical_rect(&logical_frame, &frame);
        }
        if (crt_degauss_start) {
            const uint32_t now = SDL_GetTicks();
            if (now - crt_degauss_start < crt_effect_degauss_duration()) {
                crt_effect_render_degauss(renderer, &frame, crt_degauss_start, now, crt_snapshot_tex);
                return;
            }
            crt_degauss_start = 0;
            draw_crt_snapshot(renderer, &frame, crt_snapshot_tex);
            return;
        }
        if (crt_power_on_start) {
            crt_effect_render_power_on(
                renderer, &frame, crt_power_on_start, SDL_GetTicks(), draw_crt_snapshot, crt_snapshot_tex
            );
        } else {
            crt_effect_render_power_off(
                renderer, &frame, crt_power_off_start, SDL_GetTicks(), draw_crt_snapshot, crt_snapshot_tex
            );
        }
        return;
    }

    if (!frame_tex && !hw_render_bridge_active()) return;

    const int rot = effective_rotation();

    if (rot == video_rotate_0 && !session_settings.mirrored) {
        draw_video_content(renderer, physical_output);
        return;
    }

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);

    if (!rotate_canvas_tex || rotate_canvas_w != canvas_w || rotate_canvas_h != canvas_h) {
        if (rotate_canvas_tex) SDL_DestroyTexture(rotate_canvas_tex);

        rotate_canvas_tex =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, canvas_w, canvas_h);

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
        draw_video_content(renderer, physical_output);
        return;
    }

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, rotate_canvas_tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_video_content(renderer, 0);
    SDL_SetRenderTarget(renderer, prev_target);

    const SDL_Rect logical_dst = {
        (device.mux.width - canvas_w) / 2, (device.mux.height - canvas_h) / 2, canvas_w, canvas_h
    };
    SDL_Rect final_dst = logical_dst;
    if (physical_output) display_map_logical_rect(&logical_dst, &final_dst);

    const SDL_RendererFlip flip = session_settings.mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_RenderCopyEx(renderer, rotate_canvas_tex, NULL, &final_dst, (double) rot * 90.0, NULL, flip);
}

static int ensure_output_canvas(SDL_Renderer *renderer) {
    if (output_canvas_tex && output_canvas_w == device.mux.width && output_canvas_h == device.mux.height) return 1;

    if (output_canvas_tex) SDL_DestroyTexture(output_canvas_tex);

    output_canvas_tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, device.mux.width, device.mux.height
    );
    if (!output_canvas_tex) {
        output_canvas_w = 0;
        output_canvas_h = 0;
        LOG_ERROR(mux_module, "Failed to create logical video output texture: %s", SDL_GetError());
        return 0;
    }

    SDL_SetTextureBlendMode(output_canvas_tex, SDL_BLENDMODE_BLEND);
    output_canvas_w = device.mux.width;
    output_canvas_h = device.mux.height;
    return 1;
}

static void draw_video_background(SDL_Renderer *renderer) {
    if (!display_video_needs_logical_target() || !ensure_output_canvas(renderer)) {
        draw_video_background_logical(renderer, 1);
        return;
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (SDL_SetRenderTarget(renderer, output_canvas_tex) != 0) {
        draw_video_background_logical(renderer, 1);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    draw_video_background_logical(renderer, 0);
    SDL_SetRenderTarget(renderer, previous_target);
    display_render_logical_texture(renderer, output_canvas_tex);
}

static void recompute_dest_rect(void) {
    if (frame_w == 0 || frame_h == 0) return;

    last_output_rect = (SDL_Rect) {0};

    int canvas_w, canvas_h;
    get_canvas_size(&canvas_w, &canvas_h);

    switch (session_settings.scaling_mode) {
        case video_scale_stretch:
            dest_rect.w = canvas_w;
            dest_rect.h = canvas_h;
            break;

        case video_scale_integer: {
            const int base_w = split_frame_w();
            const int base_h = split_frame_h();

            double scale;
            if (session_settings.integer_scale == integer_scale_auto) {
                const int auto_scale_w = canvas_w / base_w;
                const int auto_scale_h = canvas_h / base_h;
                int auto_scale = auto_scale_w < auto_scale_h ? auto_scale_w : auto_scale_h;
                if (auto_scale < 1) auto_scale = 1;
                scale = (double) auto_scale;
            } else {
                scale = (double) (int) session_settings_integer_scale_value(session_settings.integer_scale);
                if (scale < 1.0) scale = 1.0;
            }

            dest_rect.w = (int) (base_w * scale);
            dest_rect.h = (int) (base_h * scale);
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

        case video_scale_fit: {
            const double src_aspect = compute_src_aspect();

            dest_rect.h = canvas_h;
            dest_rect.w = (int) ((double) dest_rect.h * src_aspect);

            if (dest_rect.w > canvas_w) {
                dest_rect.w = canvas_w;
                dest_rect.h = (int) ((double) dest_rect.w / src_aspect);
            }
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

    dest_rect.w += session_settings.viewport_stretch_x;
    dest_rect.h += session_settings.viewport_stretch_y;
    if (dest_rect.w < 1) dest_rect.w = 1;
    if (dest_rect.h < 1) dest_rect.h = 1;

    dest_rect.x =
        (canvas_w - dest_rect.w) / 2 + session_settings.viewport_offset_x + overlay_bridge_viewport_offset_x();
    dest_rect.y =
        (canvas_h - dest_rect.h) / 2 + session_settings.viewport_offset_y + overlay_bridge_viewport_offset_y();

    apply_viewport_crop(canvas_w, canvas_h);
}

void video_bridge_apply_scaling(void) {
    recompute_dest_rect();
}

void video_bridge_apply_fps_limit(void) {
    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer) return;

    frame_pacer_reset_vsync_probe();

    const int software_paced = session_settings.fps_limit == fps_limit_50 || core_content_needs_pacing();
    const int want_vsync = session_settings.fps_limit == fps_limit_auto && !software_paced;

    if (SDL_RenderSetVSync(renderer, want_vsync) != 0) {
        LOG_ERROR(mux_module, "SDL_RenderSetVSync(%d) failed: %s", want_vsync, SDL_GetError());
        vsync_in_effect = 0;
    } else {
        LOG_INFO(mux_module, "SDL_RenderSetVSync(%d) applied", want_vsync);
        vsync_in_effect = want_vsync;
    }

    applied_swap_interval = -2;
    if (!SDL_GL_GetCurrentContext()) return;

    int requested_interval = want_vsync ? 1 : 0;
    if (want_vsync && hw_render_bridge_active()) requested_interval = -1;

    if (SDL_GL_SetSwapInterval(requested_interval) != 0) {
        if (requested_interval != -1 || SDL_GL_SetSwapInterval(1) != 0) {
            LOG_WARN(mux_module, "Could not apply GL swap interval %d: %s", requested_interval, SDL_GetError());
            return;
        }
        requested_interval = 1;
        LOG_INFO(mux_module, "Adaptive GL swap interval unavailable; using interval 1");
    }

    applied_swap_interval = SDL_GL_GetSwapInterval();
    // The driver has the final say. If it hands back interval zero after being asked for vsync
    // then nothing is pacing the frame, and the software deadline has to take over instead.
    if (want_vsync && applied_swap_interval == 0) {
        vsync_in_effect = 0;
        LOG_WARN(mux_module, "Vsync requested but the driver reports interval 0, pacing in software");
    }

    LOG_INFO(
        mux_module, "GL swap interval %d applied (requested %d, software paced=%d)", applied_swap_interval,
        requested_interval, software_paced
    );
}

int video_bridge_get_swap_interval(void) {
    return applied_swap_interval;
}

int video_bridge_vsync_active(void) {
    return vsync_in_effect;
}

static void apply_texture_filter(void) {
    if (!frame_tex) return;

    const int linear = texture_filter_wants_linear_sample(session_settings.texture_filter);
    SDL_SetTextureScaleMode(frame_tex, linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
}

void video_bridge_apply_filter(void) {
    apply_texture_filter();
    hw_render_bridge_apply_filter();
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

void video_bridge_get_output_geometry(
    int *source_w, int *source_h, int *logical_w, int *logical_h, int *output_w, int *output_h, int *integer_mapped
) {
    const int visible_w = crop_active ? crop_src_rect.w : split_frame_w();
    const int visible_h = crop_active ? crop_src_rect.h : split_frame_h();
    const SDL_Rect *output = last_output_rect.w > 0 && last_output_rect.h > 0 ? &last_output_rect : &dest_rect;

    if (source_w) *source_w = visible_w;
    if (source_h) *source_h = visible_h;
    if (logical_w) *logical_w = dest_rect.w;
    if (logical_h) *logical_h = dest_rect.h;
    if (output_w) *output_w = output->w;
    if (output_h) *output_h = output->h;
    if (integer_mapped) *integer_mapped = video_geometry_is_integer(output, visible_w, visible_h);
}

static void compute_target_tex_size(int *w, int *h) {
    const int scale = texture_filter_scale_factor(session_settings.texture_filter);
    const int64_t want_w = (int64_t) frame_w * scale;
    const int64_t want_h = (int64_t) frame_h * scale;
    const int within_budget = scale > 1 && want_w > 0 && want_h > 0 && want_w <= max_output_dimension
                              && want_h <= max_output_dimension && want_w * want_h <= max_output_pixels;
    cpu_filter_active = within_budget;
    *w = within_budget ? (int) want_w : frame_w;
    *h = within_budget ? (int) want_h : frame_h;

    if (scale > 1 && !within_budget && !cpu_filter_limit_logged) {
        LOG_WARN(
            mux_module, "CPU texture filter bypassed: requested %lldx%lld exceeds the 1920x1080 pixel budget",
            (long long) want_w, (long long) want_h
        );
        cpu_filter_limit_logged = 1;
    } else if (within_budget) {
        cpu_filter_limit_logged = 0;
    }
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
    if (!raw_frame_valid || !raw_frame_buf || frame_w == 0 || frame_h == 0) return;

    int want_w, want_h;
    compute_target_tex_size(&want_w, &want_h);

    if (!ensure_frame_tex(want_w, want_h, sdl_format_for_pixel_format(mux_retro_get_pixel_format()))) return;

    if (!cpu_filter_active) {
        SDL_UpdateTexture(frame_tex, NULL, raw_frame_buf, (int) raw_frame_pitch);
        return;
    }

    const size_t needed = (size_t) want_w * want_h * raw_frame_bpp;
    if (needed > scaled_buf_cap) {
        free(scaled_buf);
        scaled_buf = malloc(needed);
        scaled_buf_cap = scaled_buf ? needed : 0;
    }

    if (!scaled_buf) return;

    const int src_pitch_px = (int) (raw_frame_pitch / raw_frame_bpp);

    const uint64_t filter_start = perf_begin();
    texture_filter_apply(
        session_settings.texture_filter, raw_frame_buf, scaled_buf, frame_w, frame_h, src_pitch_px, want_w,
        raw_frame_bpp, mux_retro_get_pixel_format()
    );
    perf_end(perf_stage_texture_filter, filter_start);

    SDL_UpdateTexture(frame_tex, NULL, scaled_buf, want_w * (int) raw_frame_bpp);
}

void video_bridge_init(void) {
    display_set_video_background(draw_video_background);
}

static void draw_crt_snapshot(SDL_Renderer *renderer, const SDL_Rect *rect, void *userdata) {
    SDL_RenderCopy(renderer, userdata, NULL, rect);
}

static int capture_crt_snapshot(SDL_Renderer *renderer) {
    if (!crt_snapshot_tex) {
        crt_snapshot_tex = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, device.mux.width, device.mux.height
        );
        if (!crt_snapshot_tex) return 0;
        SDL_SetTextureBlendMode(crt_snapshot_tex, SDL_BLENDMODE_NONE);
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (SDL_SetRenderTarget(renderer, crt_snapshot_tex) != 0) return 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_video_background_logical(renderer, 0);
    SDL_SetRenderTarget(renderer, previous_target);

    return 1;
}

int video_bridge_power_on(void) {
    if (!config.visual.power_off || config.visual.reduce_motion || crt_power_off_start || crt_power_on_start) return 0;
    if (!frame_tex && !hw_render_bridge_active()) return -1;

    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer || !capture_crt_snapshot(renderer)) return 0;

    crt_power_on_start = SDL_GetTicks();
    if (!crt_power_on_start) crt_power_on_start = 1;
    display_set_ui_hidden(1);
    display_set_video_background_opaque(1);

    const uint32_t duration = crt_effect_power_off_duration();
    while (SDL_GetTicks() - crt_power_on_start < duration) {
        display_composite_frame();
        SDL_Delay(8);
    }

    crt_power_on_start = 0;
    display_composite_frame();
    return 1;
}

int video_bridge_power_off(void) {
    if (!config.visual.power_off || config.visual.reduce_motion || crt_power_off_start) return 0;
    if (!frame_tex && !hw_render_bridge_active()) return 0;

    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer || !capture_crt_snapshot(renderer)) return 0;

    crt_power_off_start = SDL_GetTicks();
    if (!crt_power_off_start) crt_power_off_start = 1;
    display_set_ui_hidden(1);
    display_set_video_background_opaque(1);

    const uint32_t duration = crt_effect_power_off_duration();
    while (SDL_GetTicks() - crt_power_off_start < duration) {
        display_composite_frame();
        SDL_Delay(8);
    }
    display_composite_frame();
    return 1;
}

int video_bridge_degauss(void) {
    if (!config.visual.power_off || config.visual.reduce_motion || crt_power_off_start || crt_power_on_start
        || crt_degauss_start)
        return 0;
    if (!frame_tex && !hw_render_bridge_active()) return 0;

    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer || !capture_crt_snapshot(renderer)) return 0;

    crt_degauss_start = SDL_GetTicks();
    if (!crt_degauss_start) crt_degauss_start = 1;
    return 1;
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
    const int normalised = (quarter_turns % video_rotate_count + video_rotate_count) % video_rotate_count;
    core_rotation_quarters = (video_rotate_count - normalised) % video_rotate_count;
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

    if (output_canvas_tex) {
        SDL_DestroyTexture(output_canvas_tex);
        output_canvas_tex = NULL;
    }

    output_canvas_w = 0;
    output_canvas_h = 0;

    if (crt_snapshot_tex) {
        SDL_DestroyTexture(crt_snapshot_tex);
        crt_snapshot_tex = NULL;
    }
    crt_power_off_start = 0;
    crt_power_on_start = 0;
    crt_degauss_start = 0;

    free(raw_frame_buf);
    raw_frame_buf = NULL;
    raw_frame_buf_cap = 0;
    raw_frame_valid = 0;

    atmosphere_ready = 0;
    atmosphere_tick = 0;

    free(scaled_buf);
    scaled_buf = NULL;
    scaled_buf_cap = 0;

    reset_anti_flicker_history(1);

    frame_w = 0;
    frame_h = 0;
    tex_w = 0;
    tex_h = 0;
    tex_format = 0;

    hw_render_bridge_shutdown();
}

void video_bridge_set_frame_skip(const int skip) {
    if (skip && !frame_skip) video_bridge_reset_temporal();
    frame_skip = skip;
}

int video_bridge_get_frame_skip(void) {
    return frame_skip;
}

static void atmosphere_sample(const void *data, const unsigned width, const unsigned height, const size_t pitch) {
    if (session_settings.border_colour != border_colour_atmosphere || !data) return;

    SDL_Rect source = {0, 0, split_frame_w(), split_frame_h()};
    if (crop_active) source = crop_src_rect;
    if (source.x < 0) source.x = 0;
    if (source.y < 0) source.y = 0;
    if (source.x + source.w > (int) width) source.w = (int) width - source.x;
    if (source.y + source.h > (int) height) source.h = (int) height - source.y;
    if (source.w < ATMOSPHERE_SEGMENTS || source.h < ATMOSPHERE_SEGMENTS) return;

    const enum retro_pixel_format format = mux_retro_get_pixel_format();
    const uint8_t *pixels = data;
    const int depth_x = source.w / 20 > 1 ? source.w / 20 : 1;
    const int depth_y = source.h / 20 > 1 ? source.h / 20 : 1;

    for (int segment = 0; segment < ATMOSPHERE_SEGMENTS; segment++) {
        const int y0 = source.y + source.h * segment / ATMOSPHERE_SEGMENTS;
        const int y1 = source.y + source.h * (segment + 1) / ATMOSPHERE_SEGMENTS;
        const int x0 = source.x + source.w * segment / ATMOSPHERE_SEGMENTS;
        const int x1 = source.x + source.w * (segment + 1) / ATMOSPHERE_SEGMENTS;
        atmosphere_target[0][segment] = atmosphere_cell(pixels, pitch, format, source.x, y0, source.x + depth_x, y1);
        atmosphere_target[1][segment] =
            atmosphere_cell(pixels, pitch, format, source.x + source.w - depth_x, y0, source.x + source.w, y1);
        atmosphere_target[2][segment] = atmosphere_cell(pixels, pitch, format, x0, source.y, x1, source.y + depth_y);
        atmosphere_target[3][segment] =
            atmosphere_cell(pixels, pitch, format, x0, source.y + source.h - depth_y, x1, source.y + source.h);
    }

    if (!atmosphere_ready) memcpy(atmosphere_current, atmosphere_target, sizeof(atmosphere_current));
    atmosphere_ready = 1;
}

static int stage_active;
static int stage_state;
static void *stage_buf;
static size_t stage_cap;
static unsigned stage_w;
static unsigned stage_h;
static size_t stage_pitch;

static int ready_state;
static void *ready_buf;
static size_t ready_cap;
static unsigned ready_w;
static unsigned ready_h;
static size_t ready_pitch;

void video_bridge_set_staging(const int staging) {
    stage_active = staging;

    if (staging) {
        stage_state = 0;
        return;
    }

    if (!stage_state) return;

    void *buf = ready_buf;
    const size_t cap = ready_cap;
    ready_buf = stage_buf;
    ready_cap = stage_cap;
    stage_buf = buf;
    stage_cap = cap;

    ready_state = stage_state;
    ready_w = stage_w;
    ready_h = stage_h;
    ready_pitch = stage_pitch;
    stage_state = 0;
}

static void video_refresh_apply(const void *data, unsigned width, unsigned height, size_t pitch);

void video_bridge_commit_staged(void) {
    if (!ready_state) return;

    const int state = ready_state;
    ready_state = 0;
    video_refresh_apply(state == 1 ? ready_buf : NULL, ready_w, ready_h, ready_pitch);
}

void mux_retro_video_refresh_cb(const void *data, const unsigned width, const unsigned height, const size_t pitch) {
    if (!stage_active) {
        video_refresh_apply(data, width, height, pitch);
        return;
    }

    stage_w = width;
    stage_h = height;
    stage_pitch = pitch;

    if (!data || data == RETRO_HW_FRAME_BUFFER_VALID || width == 0 || height == 0 || pitch > SIZE_MAX / height) {
        stage_state = data == RETRO_HW_FRAME_BUFFER_VALID ? 0 : 2;
        return;
    }

    const size_t needed = pitch * height;
    if (needed > stage_cap) {
        void *grown = malloc(needed);
        if (!grown) {
            stage_state = 0;
            return;
        }
        free(stage_buf);
        stage_buf = grown;
        stage_cap = needed;
    }

    memcpy(stage_buf, data, needed);
    stage_state = 1;
}

static void video_refresh_apply(const void *data, const unsigned width, const unsigned height, const size_t pitch) {
    if (frame_skip) return;

    perf_note_video_frame(data == NULL);

    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        if (width == 0 || height == 0) return;

        const int size_changed = (int) width != frame_w || (int) height != frame_h;
        frame_w = (int) width;
        frame_h = (int) height;
        if (size_changed) recompute_dest_rect();

        hw_render_bridge_notify_frame(width, height);
        return;
    }

    if (!data || width == 0 || height == 0) return;

    const enum retro_pixel_format pixel_format = mux_retro_get_pixel_format();
    raw_frame_bpp = bpp_for_pixel_format();
    if (pitch > INT_MAX || pitch < (size_t) width * raw_frame_bpp || (pitch > 0 && height > SIZE_MAX / pitch)) return;
    raw_frame_pitch = pitch;
    const int size_changed = (int) width != frame_w || (int) height != frame_h;
    frame_w = (int) width;
    frame_h = (int) height;
    if (size_changed) recompute_dest_rect();

    const uint64_t upload_start = perf_begin();

    // compute_target_tex_size() is what decides cpu_filter_active, so it has to run before the
    // branch that reads it. It used to be called further down, which left this frame deciding on
    // the previous frame's answer and dropping or applying the filter one frame late every time
    // the setting changed.
    int target_w = 0, target_h = 0;
    compute_target_tex_size(&target_w, &target_h);

    atmosphere_sample(data, width, height, pitch);

    if (!session_settings.anti_flicker && !cpu_filter_active) {
        raw_frame_valid = 0;
        if (ensure_frame_tex(frame_w, frame_h, sdl_format_for_pixel_format(pixel_format))) {
            SDL_UpdateTexture(frame_tex, NULL, data, (int) pitch);
            direct_upload_frames++;
        }
        perf_end(perf_stage_video_upload, upload_start);
        return;
    }

    const size_t raw_needed = pitch * height;
    if (raw_needed > raw_frame_buf_cap) {
        void *grown = malloc(raw_needed);
        if (!grown) {
            raw_frame_valid = 0;
            perf_end(perf_stage_video_upload, upload_start);
            return;
        }
        free(raw_frame_buf);
        raw_frame_buf = grown;
        raw_frame_buf_cap = raw_needed;
    }

    memcpy(raw_frame_buf, data, raw_needed);
    raw_frame_valid = 1;
    staged_upload_frames++;
    staged_upload_bytes += raw_needed;
    apply_anti_flicker(data, width, height, pitch, pixel_format);

    if (cpu_filter_active) {
        frame_dirty = 1;
        perf_end(perf_stage_video_upload, upload_start);
        return;
    }

    if (ensure_frame_tex(frame_w, frame_h, sdl_format_for_pixel_format(mux_retro_get_pixel_format())))
        SDL_UpdateTexture(frame_tex, NULL, raw_frame_buf, (int) raw_frame_pitch);

    perf_end(perf_stage_video_upload, upload_start);
}

static int video_output_is_opaque(void) {
    if (!frame_tex && !hw_render_bridge_active()) return 0;
    return 1;
}

void video_bridge_flush_frame(void) {
    display_set_video_background_opaque(video_output_is_opaque());

    if (!frame_dirty) return;
    frame_dirty = 0;
    upload_frame();
}
