#include "video.h"
#include "effects.h"
#include "../visualiser/visualiser.h"
#include "../../retro/video/filters/scale2x.h"
#include "../../retro/video/filters/super_eagle.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <module/muxshare.h>
#include <common/base/strutil.h>
#include <common/content/catalogue.h>
#include <common/content/core/common.h>
#include <common/platform/display.h>
#include "../settings/assets.h"

static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Texture *next_texture;
static SDL_Texture *vignette_texture;
static SDL_Texture *overlay_texture;
static SDL_Rect destination;
static SDL_Rect source;
static struct SwsContext *scale;
static uint8_t *planes[4];
static int strides[4];
static uint8_t *filter_source;
static uint8_t *filter_output;
static int filter_source_pitch;
static int filter_output_pitch;
static int filter_scale = 1;
static int source_width;
static int source_height;
static enum AVPixelFormat source_format = AV_PIX_FMT_NONE;
static int vignette_strength = -1;
static int vignette_width;
static int vignette_height;
static int overlay_mode = -1;
static int overlay_opacity = -1;
static int overlay_width;
static int overlay_height;
static int overlay_canvas_width;
static int overlay_canvas_height;
static char overlay_key[MAX_BUFFER_SIZE];
static char catalogue_overlay_path[PATH_MAX];
static int clean_capture;
static SDL_Texture *static_texture;
static SDL_Texture *audio_background_texture;
static int static_active;
static int static_width;
static int static_height;
static uint32_t static_deadline;
static uint32_t static_random = 0x6D2B79F5u;
static int audio_active;
static int next_texture_ready;
static Uint8 next_texture_alpha;

#define STATIC_PERIOD 33

static void create_audio_background_texture(void) {
    if (!renderer || audio_background_texture || theme.system.background_gradient_direction == LV_GRAD_DIR_NONE)
        return;
    void *pixels = NULL;
    int width = 0;
    int height = 0;
    ui_common_get_gradient_buffer(&pixels, &width, &height);
    if (!pixels || width < 1 || height < 1) return;
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        pixels, width, height, 32, width * (int) sizeof(lv_color_t), 0x00FF0000, 0x0000FF00, 0x000000FF, 0
    );
    if (!surface) return;
    audio_background_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!audio_background_texture) return;
    SDL_SetTextureBlendMode(audio_background_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(audio_background_texture, (Uint8) theme.system.background_alpha);
}

static uint32_t static_next_random(void) {
    uint32_t value = static_random;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    static_random = value;
    return value;
}

static int update_static_texture(const int force) {
    if (!renderer || !static_active) return 0;
    const uint32_t now = SDL_GetTicks();
    if (!force && !SDL_TICKS_PASSED(now, static_deadline)) return 0;

    int output_width = 0;
    int output_height = 0;
    if (SDL_GetRendererOutputSize(renderer, &output_width, &output_height) != 0
        || output_width <= 0 || output_height <= 0)
        return 0;
    const int wanted_width = output_width / 4 > 0 ? output_width / 4 : 1;
    const int wanted_height = output_height / 4 > 0 ? output_height / 4 : 1;

    if (!static_texture || static_width != wanted_width || static_height != wanted_height) {
        if (static_texture) SDL_DestroyTexture(static_texture);
        static_texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wanted_width, wanted_height
        );
        if (!static_texture) {
            static_width = 0;
            static_height = 0;
            return 0;
        }
        static_width = wanted_width;
        static_height = wanted_height;
        SDL_SetTextureScaleMode(static_texture, SDL_ScaleModeNearest);
        SDL_SetTextureBlendMode(static_texture, SDL_BLENDMODE_NONE);
    }

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(static_texture, NULL, &pixels, &pitch) != 0) return 0;
    for (int y = 0; y < static_height; y++) {
        uint32_t *row = (uint32_t *) ((uint8_t *) pixels + (size_t) y * (size_t) pitch);
        const uint8_t scanline = (y & 1) ? 8 : 0;
        for (int x = 0; x < static_width; x++) {
            const uint32_t noise = static_next_random();
            uint8_t level = (uint8_t) (24 + (noise & 95) + scanline);
            if ((noise & 0x3FFu) == 0) level = (uint8_t) (170 + ((noise >> 12) & 63));
            row[x] =
                0xFF000000u | (uint32_t) level << 16 | (uint32_t) level << 8 | level;
        }
    }
    SDL_UnlockTexture(static_texture);
    static_deadline = now + STATIC_PERIOD;
    return 1;
}

void video_render_set_content(const char *content_path, const int live) {
    catalogue_overlay_path[0] = '\0';
    if (live || !content_path || !content_path[0]) return;

    char catalogue[MAX_BUFFER_SIZE];
    get_catalogue_name_for_content(content_path, catalogue, sizeof(catalogue));
    if (!catalogue[0]) return;

    char *program = strip_ext(get_file_name(content_path));
    if (!program) return;
    load_png_catalogue(
        catalogue, program, program, "default", mux_dim, "overlay/base", catalogue_overlay_path,
        sizeof(catalogue_overlay_path)
    );
    free(program);
}

static int clamp_int(const int value, const int low, const int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static double selected_aspect(const int width, const int height) {
    switch (config.video.aspect_ratio) {
        case 1:
            return 4.0 / 3.0;
        case 2:
            return 8.0 / 7.0;
        case 3:
            return 16.0 / 9.0;
        case 4:
            return 16.0 / 10.0;
        default:
            return height > 0 ? (double) width / (double) height : 1.0;
    }
}

static void update_geometry(void) {
    if (source_width <= 0 || source_height <= 0) return;

    const int crop_left = clamp_int(source_width * config.video.crop_left / 100, 0, source_width - 1);
    const int crop_right =
        clamp_int(source_width * config.video.crop_right / 100, 0, source_width - crop_left - 1);
    const int crop_top = clamp_int(source_height * config.video.crop_top / 100, 0, source_height - 1);
    const int crop_bottom =
        clamp_int(source_height * config.video.crop_bottom / 100, 0, source_height - crop_top - 1);
    source = (SDL_Rect) {
        crop_left, crop_top, source_width - crop_left - crop_right, source_height - crop_top - crop_bottom
    };

    const int screen_width = lv_disp_get_hor_res(NULL);
    const int screen_height = lv_disp_get_ver_res(NULL);
    const int quarter_turn = config.video.rotation & 1;
    const int canvas_width = quarter_turn ? screen_height : screen_width;
    const int canvas_height = quarter_turn ? screen_width : screen_height;
    const int viewport_width = source.w;
    const int viewport_height = source.h;
    const double aspect = selected_aspect(viewport_width, viewport_height);

    int width = canvas_width;
    int height = canvas_height;
    switch (config.video.scaling_mode) {
        case 1: {
            int multiplier = config.video.scale_multiplier;
            if (multiplier <= 0) {
                const int horizontal = canvas_width / viewport_width;
                const int vertical = canvas_height / viewport_height;
                multiplier = horizontal < vertical ? horizontal : vertical;
                if (multiplier < 1) multiplier = 1;
            }
            width = viewport_width * multiplier;
            height = viewport_height * multiplier;
            break;
        }
        case 2:
            width = canvas_width;
            height = canvas_height;
            break;
        case 3:
            height = canvas_height;
            width = (int) lround((double) height * aspect);
            break;
        case 4:
            width = canvas_width;
            height = (int) lround((double) width / aspect);
            break;
        case 5:
            height = canvas_height;
            width = (int) lround((double) height * aspect);
            if (width > canvas_width) {
                width = canvas_width;
                height = (int) lround((double) width / aspect);
            }
            break;
        case 0:
        default: {
            int multiplier = config.video.scale_multiplier;
            if (multiplier <= 0) {
                multiplier = canvas_height / viewport_height;
                if (multiplier < 1) multiplier = 1;
                while (multiplier > 1 && (double) viewport_height * multiplier * aspect > canvas_width)
                    multiplier--;
            }
            height = viewport_height * multiplier;
            width = (int) lround((double) height * aspect);
            break;
        }
    }

    width = width * config.video.viewport_zoom / 100;
    height = height * config.video.viewport_zoom / 100;
    width += canvas_width * config.video.viewport_stretch_x / 100;
    height += canvas_height * config.video.viewport_stretch_y / 100;
    destination.w = width;
    destination.h = height;
    if (destination.w < 1) destination.w = 1;
    if (destination.h < 1) destination.h = 1;
    destination.x = (screen_width - width) / 2 + screen_width * config.video.viewport_x / 100;
    destination.y = (screen_height - height) / 2 + screen_height * config.video.viewport_y / 100;
    if (!config.video.viewport_centre_crop) {
        destination.x += (int) lround((double) (crop_left - crop_right) * width / (2.0 * viewport_width));
        destination.y += (int) lround((double) (crop_top - crop_bottom) * height / (2.0 * viewport_height));
    }
}

static void update_vignette(void) {
    const int screen_width = lv_disp_get_hor_res(NULL);
    const int screen_height = lv_disp_get_ver_res(NULL);
    const int width = destination.w < screen_width ? destination.w : screen_width;
    const int height = destination.h < screen_height ? destination.h : screen_height;
    const int strength = config.video.vignette_strength;
    const int shape = config.video.vignette_shape;
    if (width <= 0 || height <= 0 || strength <= 0 || shape <= 0) {
        if (vignette_texture) SDL_DestroyTexture(vignette_texture);
        vignette_texture = NULL;
        vignette_strength = strength;
        return;
    }
    if (vignette_texture && vignette_strength == strength && vignette_width == width && vignette_height == height)
        return;

    if (vignette_texture) SDL_DestroyTexture(vignette_texture);
    vignette_texture = NULL;
    uint32_t *pixels = malloc((size_t) width * (size_t) height * sizeof(*pixels));
    if (!pixels) return;

    const double inverse_x = width > 1 ? 2.0 / (double) (width - 1) : 0.0;
    const double inverse_y = height > 1 ? 2.0 / (double) (height - 1) : 0.0;
    const double shorter = width < height ? width : height;
    const double aspect_x = config.video.vignette_scaling ? (double) width / shorter : 1.0;
    const double aspect_y = config.video.vignette_scaling ? (double) height / shorter : 1.0;
    const double width_scale = 100.0 * aspect_x / config.video.vignette_width;
    const double height_scale = 100.0 * aspect_y / config.video.vignette_height;
    const double offset_x = (double) config.video.vignette_offset_x / 100.0;
    const double offset_y = (double) config.video.vignette_offset_y / 100.0;
    const double softness = fmax(0.01, (double) config.video.vignette_softness / 100.0);
    const uint32_t colour = config.video.vignette_colour == 1 ? 0x00FFFFFFu : 0;
    for (int y = 0; y < height; y++) {
        const double ny = ((double) y * inverse_y - 1.0 - offset_y * 2.0) * height_scale;
        for (int x = 0; x < width; x++) {
            const double nx = ((double) x * inverse_x - 1.0 - offset_x * 2.0) * width_scale;
            double distance;
            if (shape == 2) {
                const double qx = pow(fmin(fabs(nx), 1.5), 16.0);
                const double qy = pow(fmin(fabs(ny), 1.5), 16.0);
                distance = pow(qx + qy, 1.0 / 16.0);
            } else if (shape == 3) {
                const double qx = pow(fmin(fabs(nx), 1.5), 4.0);
                const double qy = pow(fmin(fabs(ny), 1.5), 4.0);
                distance = pow(qx + qy, 0.25);
            } else if (shape == 4) {
                const double segment = 1.2566371;
                double angle = fmod(atan2(ny, nx) + 6.2831853, segment) - segment * 0.5;
                distance = hypot(nx, ny) / (1.0 - 0.7 * (fabs(angle) / (segment * 0.5)));
            } else if (shape == 5) {
                const double q = ny * 1.5 - 0.5;
                distance = fmax(fabs(nx) * 1.2 - q * 0.5, q);
            } else {
                distance = hypot(nx, ny);
            }
            const double start = 1.0 - softness;
            double fade = distance <= start ? 0.0 : fmin(1.0, (distance - start) / softness);
            fade = fade * fade * (3.0 - 2.0 * fade);
            const uint8_t alpha = (uint8_t) lround(fade * (double) strength * 2.55);
            pixels[(size_t) y * width + x] = colour | (uint32_t) alpha << 24;
        }
    }

    vignette_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (vignette_texture) {
        SDL_UpdateTexture(vignette_texture, NULL, pixels, width * (int) sizeof(*pixels));
        SDL_SetTextureBlendMode(vignette_texture, SDL_BLENDMODE_BLEND);
    }
    free(pixels);
    vignette_strength = strength;
    vignette_width = width;
    vignette_height = height;
}

static void update_overlay(void) {
    const int width = lv_disp_get_hor_res(NULL);
    const int height = lv_disp_get_ver_res(NULL);
    const int mode = config.video.overlay_mode == 1 ? config.video.overlay_pattern + 1
                     : config.video.overlay_mode == 2 ? 100
                     : config.video.overlay_mode == 3 ? 101
                                                      : 0;
    const int opacity = config.video.overlay_opacity;
    if (mode <= 0 || opacity <= 0) {
        if (overlay_texture) SDL_DestroyTexture(overlay_texture);
        overlay_texture = NULL;
        overlay_mode = mode;
        overlay_opacity = opacity;
        overlay_key[0] = '\0';
        return;
    }
    const char *selected_key = mode == 100 ? catalogue_overlay_path
                               : mode == 101 ? config.video.overlay_image
                                             : "";
    if (overlay_texture && overlay_mode == mode && overlay_opacity == opacity && overlay_canvas_width == width
        && overlay_canvas_height == height && strcmp(overlay_key, selected_key) == 0)
        return;

    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    overlay_texture = NULL;
    overlay_width = 0;
    overlay_height = 0;

    if (mode == 100 || mode == 101) {
        const char *path = catalogue_overlay_path;
        int selected = path[0] != '\0';
        if (mode == 101) {
            wasabi_assets_refresh(wasabi_asset_overlay);
            selected = wasabi_asset_selected(wasabi_asset_overlay);
            path = wasabi_asset_path(wasabi_asset_overlay, selected);
        }
        if (selected > 0 && path && path[0]) overlay_texture = IMG_LoadTexture(renderer, path);
        if (overlay_texture) {
            SDL_QueryTexture(overlay_texture, NULL, NULL, &overlay_width, &overlay_height);
            SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(overlay_texture, (Uint8) lround((double) opacity * 2.55));
        } else {
            LOG_WARN(mux_module, "Unable to load Wasabi overlay: %s", path ? path : "");
        }
        overlay_mode = mode;
        overlay_opacity = opacity;
        overlay_canvas_width = width;
        overlay_canvas_height = height;
        snprintf(overlay_key, sizeof(overlay_key), "%s", selected_key);
        return;
    }

    uint32_t *pixels = calloc((size_t) width * (size_t) height, sizeof(*pixels));
    if (!pixels) return;
    const uint8_t alpha = (uint8_t) lround((double) opacity * 2.55 * 0.55);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int line = 0;
            switch (mode - 1) {
                case 0: line = ((x ^ y) & 1); break;
                case 1: line = (((x >> 2) ^ (y >> 2)) & 1); break;
                case 2: line = ((x + y) & 1) == 0; break;
                case 3: line = ((x + y) & 3) == 0; break;
                case 4: line = ((x + y) & 7) == 0; break;
                case 5: line = (x & 1) || (y & 1); break;
                case 6: line = (x & 3) == 3 || (y & 3) == 3; break;
                case 7: line = (y & 1); break;
                case 8: line = (y & 3) == 3; break;
                case 9: line = (y & 7) == 7; break;
                case 10: line = (x & 1); break;
                case 11: line = (x & 3) == 3; break;
                default: line = (x & 7) == 7; break;
            }
            if (line) pixels[(size_t) y * width + x] = (uint32_t) alpha << 24;
        }
    }
    overlay_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (overlay_texture) {
        SDL_UpdateTexture(overlay_texture, NULL, pixels, width * (int) sizeof(*pixels));
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
    }
    free(pixels);
    overlay_mode = mode;
    overlay_opacity = opacity;
    overlay_width = width;
    overlay_height = height;
    overlay_canvas_width = width;
    overlay_canvas_height = height;
    overlay_key[0] = '\0';
}

static void render_frame(SDL_Renderer *target) {
    SDL_SetRenderDrawBlendMode(target, SDL_BLENDMODE_NONE);
    if (static_active) {
        SDL_SetRenderDrawColor(target, 20, 20, 20, 255);
        SDL_RenderClear(target);
        update_static_texture(0);
        if (static_texture) SDL_RenderCopy(target, static_texture, NULL, NULL);
        return;
    }
    if (audio_active) {
        create_audio_background_texture();
        if (audio_background_texture) SDL_RenderCopy(target, audio_background_texture, NULL, NULL);
        wasabi_visualiser_render(target);
        return;
    }
    const SDL_Color borders[] = {{0, 0, 0, 255}, {0, 0, 0, 255}, {32, 32, 32, 255}, {255, 255, 255, 255}};
    const SDL_Color border = borders[clamp_int(config.video.border_colour, 0, 3)];
    SDL_SetRenderDrawColor(target, border.r, border.g, border.b, border.a);
    SDL_RenderClear(target);
    if (texture) {
        const SDL_RendererFlip flip = config.video.mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_Rect texture_source = source;
        if (filter_scale > 1) {
            texture_source.x *= filter_scale;
            texture_source.y *= filter_scale;
            texture_source.w *= filter_scale;
            texture_source.h *= filter_scale;
        }
        const int effects_rendered = !clean_capture
                                     && video_effects_render(
                target, texture, next_texture_ready ? next_texture : NULL, next_texture_alpha, &texture_source,
                &destination, config.video.rotation * 90.0, flip
            );
        if (clean_capture || !effects_rendered) {
            SDL_RenderCopyEx(target, texture, &texture_source, &destination, config.video.rotation * 90.0, NULL, flip);
            if (!clean_capture && next_texture_ready && next_texture_alpha > 0) {
                SDL_SetTextureAlphaMod(next_texture, next_texture_alpha);
                SDL_RenderCopyEx(
                    target, next_texture, &texture_source, &destination, config.video.rotation * 90.0, NULL, flip
                );
            }
        }
    }
    if (clean_capture) return;
    update_vignette();
    if (vignette_texture) SDL_RenderCopy(target, vignette_texture, NULL, &destination);
    update_overlay();
    if (overlay_texture) {
        int left = config.video.overlay_crop_left;
        int right = config.video.overlay_crop_right;
        int top = config.video.overlay_crop_top;
        int bottom = config.video.overlay_crop_bottom;
        SDL_Rect overlay_source = {
            overlay_width * left / 100, overlay_height * top / 100,
            overlay_width * (100 - left - right) / 100, overlay_height * (100 - top - bottom) / 100
        };
        int full_width = device.mux.width * config.video.overlay_zoom / 100
                         + device.mux.width * config.video.overlay_stretch_x / 100;
        int full_height = device.mux.height * config.video.overlay_zoom / 100
                          + device.mux.height * config.video.overlay_stretch_y / 100;
        const int left_px = full_width * left / 100;
        const int right_px = full_width * right / 100;
        const int top_px = full_height * top / 100;
        const int bottom_px = full_height * bottom / 100;
        SDL_Rect overlay_destination = {
            (device.mux.width - full_width) / 2 + left_px + device.mux.width * config.video.overlay_x / 100,
            (device.mux.height - full_height) / 2 + top_px + device.mux.height * config.video.overlay_y / 100,
            full_width - left_px - right_px, full_height - top_px - bottom_px
        };
        if (config.video.overlay_centre_crop) {
            overlay_destination.x = (device.mux.width - overlay_destination.w) / 2
                                    + device.mux.width * config.video.overlay_x / 100;
            overlay_destination.y = (device.mux.height - overlay_destination.h) / 2
                                    + device.mux.height * config.video.overlay_y / 100;
        }
        if (overlay_source.w > 0 && overlay_source.h > 0 && overlay_destination.w > 0 && overlay_destination.h > 0)
            SDL_RenderCopy(target, overlay_texture, &overlay_source, &overlay_destination);
    }
}

static void release_frame_resources(void) {
    if (texture) SDL_DestroyTexture(texture);
    texture = NULL;
    if (next_texture) SDL_DestroyTexture(next_texture);
    next_texture = NULL;
    next_texture_ready = 0;
    next_texture_alpha = 0;
    if (scale) sws_freeContext(scale);
    scale = NULL;
    if (planes[0]) av_freep(&planes[0]);
    filter_source = NULL;
    av_freep(&filter_output);
    for (size_t i = 0; i < 4; ++i) planes[i] = NULL;
    for (size_t i = 0; i < 4; ++i) strides[i] = 0;
    source_width = 0;
    source_height = 0;
    source_format = AV_PIX_FMT_NONE;
    filter_source_pitch = filter_output_pitch = 0;
    filter_scale = 1;
}

static int requested_filter_scale(const int width, const int height) {
    int factor = 1;
    if (config.video.texture_filter == 2 || config.video.texture_filter == 5
        || config.video.texture_filter == 6)
        factor = 2;
    else if (config.video.texture_filter == 3)
        factor = 3;
    if (factor > 1
        && ((int64_t) width * factor > 1920 || (int64_t) height * factor > 1920
            || (int64_t) width * height * factor * factor > (int64_t) 1920 * 1080))
        factor = 1;
    return factor;
}

static void update_texture_scale_mode(void) {
    if (!texture) return;
    const SDL_ScaleMode mode = config.video.texture_filter == 1 || config.video.texture_filter == 4
                                       || config.video.texture_filter == 5
                                   ? SDL_ScaleModeLinear
                                   : SDL_ScaleModeNearest;
    SDL_SetTextureScaleMode(texture, mode);
    if (next_texture) SDL_SetTextureScaleMode(next_texture, mode);
}

static SDL_Texture *create_frame_texture(void) {
    SDL_Texture *created = SDL_CreateTexture(
        renderer, filter_scale > 1 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING, source_width * filter_scale, source_height * filter_scale
    );
    if (!created) return NULL;
    SDL_SetTextureScaleMode(
        created, config.video.texture_filter == 1 || config.video.texture_filter == 4
                         || config.video.texture_filter == 5
                     ? SDL_ScaleModeLinear
                     : SDL_ScaleModeNearest
    );
    return created;
}

static int configure_frame(const AVFrame *frame) {
    release_frame_resources();
    if (!renderer || !frame || frame->width <= 0 || frame->height <= 0) return 0;

    source_width = frame->width;
    source_height = frame->height;
    source_format = (enum AVPixelFormat) frame->format;
    filter_scale = requested_filter_scale(source_width, source_height);
    const int cpu_filter = filter_scale > 1;
    texture = create_frame_texture();
    if (!texture) {
        release_frame_resources();
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    source = (SDL_Rect) {0, 0, source_width, source_height};
    update_geometry();

    if (cpu_filter) {
        filter_source_pitch = source_width * 4;
        filter_output_pitch = source_width * filter_scale * 4;
        filter_source = av_malloc((size_t) filter_source_pitch * source_height);
        filter_output = av_malloc((size_t) filter_output_pitch * source_height * filter_scale);
        if (!filter_source || !filter_output) {
            release_frame_resources();
            return 0;
        }
        planes[0] = filter_source;
        strides[0] = filter_source_pitch;
        scale = sws_getContext(source_width, source_height, source_format, source_width, source_height,
                               AV_PIX_FMT_BGRA, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        if (!scale) release_frame_resources();
        return scale != NULL;
    }

    if (source_format == AV_PIX_FMT_YUV420P || source_format == AV_PIX_FMT_YUVJ420P) return 1;

    if (av_image_alloc(planes, strides, source_width, source_height, AV_PIX_FMT_YUV420P, 32) < 0) {
        release_frame_resources();
        return 0;
    }
    scale = sws_getContext(
        source_width, source_height, source_format, source_width, source_height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
        NULL, NULL, NULL
    );
    if (!scale) release_frame_resources();
    return scale != NULL;
}

int video_render_open(void) {
    renderer = display_get_renderer();
    if (!renderer) return 0;
    if (audio_active) create_audio_background_texture();
    display_set_video_background(render_frame);
    display_set_video_background_opaque(!audio_active);
    return 1;
}

void video_render_set_audio(const int active) {
    audio_active = active != 0;
    if (renderer) display_set_video_background_opaque(!audio_active);
    wasabi_visualiser_reset();
}

void video_render_audio_samples(const float *samples, const int frames, const int channels) {
    if (audio_active) wasabi_visualiser_capture(samples, frames, channels);
}

int video_render_audio_tick(void) {
    return audio_active && wasabi_visualiser_tick();
}

static int upload_frame(SDL_Texture *target, const AVFrame *frame) {
    if (!target || !frame) return 0;

    const uint8_t *const *data = (const uint8_t *const *) frame->data;
    const int *line = frame->linesize;
    if (scale) {
        if (sws_scale(scale, data, line, 0, frame->height, planes, strides) <= 0) return 0;
        data = (const uint8_t *const *) planes;
        line = strides;
    }

    if (filter_scale > 1) {
        if (config.video.texture_filter == 6)
            super_eagle_32((const uint32_t *) filter_source, (uint32_t *) filter_output, source_width,
                           source_height, filter_source_pitch / 4, filter_output_pitch / 4);
        else if (filter_scale == 3)
            scale3_x_32((const uint32_t *) filter_source, (uint32_t *) filter_output, source_width, source_height,
                        filter_source_pitch / 4, filter_output_pitch / 4);
        else
            scale2_x_32((const uint32_t *) filter_source, (uint32_t *) filter_output, source_width, source_height,
                        filter_source_pitch / 4, filter_output_pitch / 4);
        return SDL_UpdateTexture(target, NULL, filter_output, filter_output_pitch) == 0;
    }
    return SDL_UpdateYUVTexture(target, NULL, data[0], line[0], data[1], line[1], data[2], line[2]) == 0;
}

int video_render_upload(const AVFrame *frame) {
    if (!frame) return 0;
    const int wanted_scale = requested_filter_scale(frame->width, frame->height);
    if (!texture || source_width != frame->width || source_height != frame->height
        || source_format != (enum AVPixelFormat) frame->format || filter_scale != wanted_scale) {
        if (!configure_frame(frame)) return 0;
    }
    next_texture_ready = 0;
    next_texture_alpha = 0;
    return upload_frame(texture, frame);
}

int video_render_upload_next(const AVFrame *frame) {
    if (!frame || !texture || source_width != frame->width || source_height != frame->height
        || source_format != (enum AVPixelFormat) frame->format)
        return 0;
    if (!next_texture) {
        next_texture = create_frame_texture();
        if (!next_texture) return 0;
        SDL_SetTextureBlendMode(next_texture, SDL_BLENDMODE_BLEND);
    }
    next_texture_ready = upload_frame(next_texture, frame);
    return next_texture_ready;
}

int video_render_promote_next(void) {
    if (!texture || !next_texture || !next_texture_ready) return 0;
    SDL_Texture *previous = texture;
    texture = next_texture;
    next_texture = previous;
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(next_texture, 255);
    SDL_SetTextureBlendMode(next_texture, SDL_BLENDMODE_BLEND);
    next_texture_ready = 0;
    next_texture_alpha = 0;
    return 1;
}

void video_render_set_blend(const double amount) {
    if (!next_texture_ready || amount <= 0.0) {
        next_texture_alpha = 0;
        return;
    }
    const double clamped = amount < 1.0 ? amount : 1.0;
    next_texture_alpha = (Uint8) lround(clamped * 255.0);
}

void video_render_settings_changed(void) {
    update_geometry();
    vignette_strength = -1;
    overlay_mode = -1;
    overlay_key[0] = '\0';
    update_texture_scale_mode();
    display_composite_frame();
}

void video_render_effects_changed(void) {
    video_effects_changed();
    display_composite_frame();
}

void video_render_set_clean_capture(const int active) {
    clean_capture = active != 0;
}

void video_render_set_static(const int active) {
    const int enabled = active != 0;
    if (static_active == enabled) return;
    static_active = enabled;
    if (static_active) {
        static_deadline = 0;
        update_static_texture(1);
    }
}

int video_render_static_tick(void) {
    return update_static_texture(0);
}

void video_render_close(void) {
    display_clear_video_background();
    if (audio_background_texture) SDL_DestroyTexture(audio_background_texture);
    audio_background_texture = NULL;
    if (static_texture) SDL_DestroyTexture(static_texture);
    static_texture = NULL;
    static_width = 0;
    static_height = 0;
    static_active = 0;
    audio_active = 0;
    wasabi_visualiser_reset();
    static_deadline = 0;
    if (vignette_texture) SDL_DestroyTexture(vignette_texture);
    vignette_texture = NULL;
    vignette_strength = -1;
    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    overlay_texture = NULL;
    overlay_mode = -1;
    video_effects_close();
    release_frame_resources();
    renderer = NULL;
}
