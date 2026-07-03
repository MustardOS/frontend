#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../lvgl/src/draw/sdl/lv_draw_sdl.h"
#include "../lvgl/src/draw/sdl/lv_draw_sdl_texture_cache.h"
#include "anim.h"
#include "log.h"
#include "init.h"
#include "device.h"
#include "input.h"
#include "config.h"
#include "theme.h"
#include "display.h"
#include "ui/common.h"
#include "inotify.h"
#include "fileio.h"
#include "saver.h"
#include "video.h"
#include "saver/dvd.h"
#include "saver/star.h"
#include "saver/matrix.h"
#include "saver/firefly.h"
#include "saver/pulse.h"
#include "saver/trace.h"
#include "saver/constellation.h"
#include "saver/mystify.h"
#include "saver/maze.h"
#include "saver/blockfall.h"
#include "saver/datetime.h"
#include "saver/slideshow.h"
#include "saver/boxart.h"
#include "saver/bsod.h"

typedef struct {
    // Just the default base stuff we need
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    volatile int refresh;

    // Cached render state
    SDL_Rect dest_rect;
    double angle;
    SDL_Point pivot;
    SDL_Point *pivot_ptr;

    // Track if we should clear
    int needs_clear;
    int force_clear;

    // Background image support
    int background_solid;
    SDL_Texture *background_image;
    SDL_Color background_colour;
    char theme_name[256];

    // Theme overlay image
    SDL_Texture *theme_overlay;
    uint8_t theme_overlay_opacity;

    // Shadow layer composited between background and UI texture
    SDL_Texture *shadow_layer;
} monitor_t;

static monitor_t monitor;

static uint32_t last_saver_exit = 0;
static uint8_t display_fade_alpha = 0;
static int gradient_captured = 0;
static display_overlay_fn video_overlay_fn_ptr = NULL;
static display_overlay_fn video_bg_fn_ptr = NULL;

SDL_Renderer *display_get_renderer(void) {
    return monitor.renderer;
}

SDL_Texture *display_get_shadow_layer(void) {
    return monitor.shadow_layer;
}

void display_set_video_overlay(const display_overlay_fn fn) {
    video_overlay_fn_ptr = fn;
}

void display_clear_video_overlay(void) {
    video_overlay_fn_ptr = NULL;
}

void display_set_video_background(const display_overlay_fn fn) {
    video_bg_fn_ptr = fn;
}

void display_clear_video_background(void) {
    video_bg_fn_ptr = NULL;
}

void display_set_fade_alpha(const uint8_t alpha) {
    display_fade_alpha = alpha;
}

int scale_width, scale_height, underscan;

static void update_blend_mode(void) {
    SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawBlendMode(monitor.renderer, SDL_BLENDMODE_NONE);

    monitor.force_clear = 1;
}

static SDL_Texture *load_png(SDL_Renderer *renderer, const char *path) {
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        LOG_ERROR("video", "PNG loading failure: %s", IMG_GetError());
        return NULL;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (!tex) LOG_ERROR("video", "Texture creation failure: %s", SDL_GetError());
    return tex;
}

SDL_Texture *display_load_png_texture(const char *path) {
    return load_png(monitor.renderer, path);
}

void display_set_theme_overlay(SDL_Texture *tex, const uint8_t opacity) {
    if (monitor.theme_overlay) SDL_DestroyTexture(monitor.theme_overlay);

    monitor.theme_overlay = tex;
    monitor.theme_overlay_opacity = opacity;
}

void display_update_overlay_opacity(const uint8_t opacity) {
    monitor.theme_overlay_opacity = opacity;
}

void display_clear_theme_overlay(void) {
    if (monitor.theme_overlay) {
        SDL_DestroyTexture(monitor.theme_overlay);
        monitor.theme_overlay = NULL;
    }

    monitor.theme_overlay_opacity = 0;
}

static void reload_background(const char *active_theme) {
    char back_image[MAX_BUFFER_SIZE];
    snprintf(back_image, sizeof(back_image), "%s/%simage/background.png", theme_base, mux_dim);

    if (!file_exist(back_image)) {
        snprintf(back_image, sizeof(back_image), "%s/image/background.png", theme_base);
        if (!file_exist(back_image)) {
            if (monitor.background_image) {
                SDL_DestroyTexture(monitor.background_image);
                monitor.background_image = NULL;
            }

            // If the theme background opacity is set to 0 we'll just revert
            // back to a solid black colour, otherwise we'll get some lovely
            // overlap graphic glitches!
            monitor.background_solid = 1;
            monitor.background_colour = (SDL_Color) {theme.sdl.solid.r, theme.sdl.solid.g, theme.sdl.solid.b, 255};
            monitor.theme_name[0] = '\0';

            // The good news is that a failure to find an image fails gracefully!
            LOG_INFO("video", "No 'background.png' found for theme '%s'", active_theme);
            return;
        }
    }

    if (monitor.background_image && strcmp(active_theme, monitor.theme_name) == 0) return;

    if (monitor.background_image) {
        SDL_DestroyTexture(monitor.background_image);
        monitor.background_image = NULL;
    }

    monitor.background_image = load_png(monitor.renderer, back_image);

    if (!monitor.background_image) {
        monitor.theme_name[0] = '\0';
        return;
    }

    snprintf(monitor.theme_name, sizeof(monitor.theme_name), "%s", active_theme);
    LOG_INFO("video", "Loaded theme background: %s", back_image);
}

enum {
    saver_type_disabled = 0,
    saver_type_dvd = 1,
    saver_type_star = 2,
    saver_type_matrix = 3,
    saver_type_firefly = 4,
    saver_type_pulse = 5,
    saver_type_trace = 6,
    saver_type_constellation = 7,
    saver_type_mystify = 8,
    saver_type_maze = 9,
    saver_type_blockfall = 10,
    saver_type_datetime = 11,
    saver_type_video = 12,
    saver_type_slideshow = 13,
    saver_type_boxart = 14,
    saver_type_bsod = 15,
};

static char video_saver_path[MAX_BUFFER_SIZE];
static char video_saver_prev_path[MAX_BUFFER_SIZE];
static int video_saver_running = 0;
static saver_state_t video_saver_base;

static int saver_speed_override = 0;
static int active_saver = -1;

static int saver_active(void) {
    if (active_saver == saver_type_bsod) return bsod_active();
    if (active_saver == saver_type_boxart) return boxart_active();
    if (active_saver == saver_type_slideshow) return slideshow_active();
    if (active_saver == saver_type_video) return saver_active_base(&video_saver_base);
    if (active_saver == saver_type_datetime) return datetime_active();
    if (active_saver == saver_type_blockfall) return blockfall_active();
    if (active_saver == saver_type_maze) return maze_active();
    if (active_saver == saver_type_mystify) return mystify_active();
    if (active_saver == saver_type_constellation) return constellation_active();
    if (active_saver == saver_type_trace) return trace_active();
    if (active_saver == saver_type_pulse) return pulse_active();
    if (active_saver == saver_type_firefly) return firefly_active();
    if (active_saver == saver_type_matrix) return matrix_active();
    if (active_saver == saver_type_star) return star_active();
    if (active_saver == saver_type_dvd) return dvd_active();
    return 0;
}

static void saver_update(void) {
    if (active_saver == saver_type_video) {
        if (saver_poll_idle(&video_saver_base, SDL_GetTicks()) && !video_saver_running) {
            snprintf(
                video_saver_prev_path, sizeof(video_saver_prev_path), "%s",
                video_wallpaper_active() ? video_wallpaper_path() : ""
            );
            video_wallpaper_play(video_saver_path);
            video_saver_running = 1;
        }
        return;
    }
    if (active_saver == saver_type_datetime)
        datetime_update();
    else if (active_saver == saver_type_blockfall)
        blockfall_update();
    else if (active_saver == saver_type_maze)
        maze_update();
    else if (active_saver == saver_type_mystify)
        mystify_update();
    else if (active_saver == saver_type_constellation)
        constellation_update();
    else if (active_saver == saver_type_trace)
        trace_update();
    else if (active_saver == saver_type_pulse)
        pulse_update();
    else if (active_saver == saver_type_firefly)
        firefly_update();
    else if (active_saver == saver_type_matrix)
        matrix_update();
    else if (active_saver == saver_type_star)
        star_update();
    else if (active_saver == saver_type_dvd)
        dvd_update();
    else if (active_saver == saver_type_slideshow)
        slideshow_update();
    else if (active_saver == saver_type_boxart)
        boxart_update();
    else if (active_saver == saver_type_bsod)
        bsod_update();
}

static void saver_render(SDL_Renderer *r) {
    if (active_saver == saver_type_video) {
        video_wallpaper_render_frame(r);
        return;
    }
    if (active_saver == saver_type_datetime)
        datetime_render(r);
    else if (active_saver == saver_type_blockfall)
        blockfall_render(r);
    else if (active_saver == saver_type_maze)
        maze_render(r);
    else if (active_saver == saver_type_mystify)
        mystify_render(r);
    else if (active_saver == saver_type_constellation)
        constellation_render(r);
    else if (active_saver == saver_type_trace)
        trace_render(r);
    else if (active_saver == saver_type_pulse)
        pulse_render(r);
    else if (active_saver == saver_type_firefly)
        firefly_render(r);
    else if (active_saver == saver_type_matrix)
        matrix_render(r);
    else if (active_saver == saver_type_star)
        star_render(r);
    else if (active_saver == saver_type_dvd)
        dvd_render(r);
    else if (active_saver == saver_type_slideshow)
        slideshow_render(r);
    else if (active_saver == saver_type_boxart)
        boxart_render(r);
    else if (active_saver == saver_type_bsod)
        bsod_render(r);
}

static void saver_stop(void) {
    if (active_saver == saver_type_video) {
        video_wallpaper_stop();
        if (video_saver_prev_path[0]) {
            video_wallpaper_play(video_saver_prev_path);
            video_saver_prev_path[0] = '\0';
        }
        video_saver_running = 0;
        saver_stop_base(&video_saver_base);
        return;
    }
    if (active_saver == saver_type_datetime)
        datetime_stop();
    else if (active_saver == saver_type_blockfall)
        blockfall_stop();
    else if (active_saver == saver_type_maze)
        maze_stop();
    else if (active_saver == saver_type_mystify)
        mystify_stop();
    else if (active_saver == saver_type_constellation)
        constellation_stop();
    else if (active_saver == saver_type_trace)
        trace_stop();
    else if (active_saver == saver_type_pulse)
        pulse_stop();
    else if (active_saver == saver_type_firefly)
        firefly_stop();
    else if (active_saver == saver_type_matrix)
        matrix_stop();
    else if (active_saver == saver_type_star)
        star_stop();
    else if (active_saver == saver_type_dvd)
        dvd_stop();
    else if (active_saver == saver_type_slideshow)
        slideshow_stop();
    else if (active_saver == saver_type_boxart)
        boxart_stop();
    else if (active_saver == saver_type_bsod)
        bsod_stop();
}

int get_saver_speed(const int fallback) {
    int speed = saver_speed_override;

    if (speed <= 0) speed = config.settings.power.saver_speed;
    if (speed <= 0) speed = read_line_int_from(CONF_CONFIG_PATH "settings/power/saver_speed", fallback);
    if (speed <= 0) speed = fallback;

    return speed;
}

static void reload_saver() {
    if (active_saver == saver_type_video && video_saver_running) {
        video_wallpaper_stop();
        if (video_saver_prev_path[0]) {
            video_wallpaper_play(video_saver_prev_path);
            video_saver_prev_path[0] = '\0';
        }
        video_saver_running = 0;
    }

    dvd_shutdown();
    star_shutdown();
    matrix_shutdown();
    firefly_shutdown();
    pulse_shutdown();
    trace_shutdown();
    constellation_shutdown();
    mystify_shutdown();
    maze_shutdown();
    blockfall_shutdown();
    datetime_shutdown();
    slideshow_shutdown();
    boxart_shutdown();
    bsod_shutdown();

    const int type = config.settings.power.saver_type;

    if (type == saver_type_datetime) {
        const uint32_t tc = theme.list_default.text;
        const uint8_t dt_r = (uint8_t) (tc >> 16 & 0xFF);
        const uint8_t dt_g = (uint8_t) (tc >> 8 & 0xFF);
        const uint8_t dt_b = (uint8_t) (tc & 0xFF);
        const uint8_t dt_a = theme.list_default.text_alpha < 0 ? 255 : (uint8_t) theme.list_default.text_alpha;
        datetime_init(monitor.renderer, device.screen.width, device.screen.height, dt_r, dt_g, dt_b, dt_a);
    } else if (type == saver_type_blockfall) {
        blockfall_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_maze) {
        maze_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_mystify) {
        mystify_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_constellation) {
        constellation_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_trace) {
        trace_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_pulse) {
        pulse_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_firefly) {
        firefly_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_matrix) {
        matrix_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_star) {
        star_init(monitor.renderer, device.screen.width, device.screen.height);
    } else if (type == saver_type_video) {
        char saver_video[MAX_BUFFER_SIZE];
        snprintf(saver_video, sizeof(saver_video), "%s/%simage/screensaver.mp4", theme_base, mux_dim);

        if (!file_exist(saver_video)) {
            snprintf(saver_video, sizeof(saver_video), "%s/image/screensaver.mp4", theme_base);
            if (!file_exist(saver_video)) snprintf(saver_video, sizeof(saver_video), OPT_SHARE_PATH "media/logo.mp4");
        }

        snprintf(video_saver_path, sizeof(video_saver_path), "%s", saver_video);
        saver_init_base(
            &video_saver_base, device.screen.width, device.screen.height, "Video Wallpaper", 0, 0, 0, NULL, NULL, NULL
        );
    } else if (type == saver_type_dvd) {
        char saver_image[MAX_BUFFER_SIZE];
        snprintf(saver_image, sizeof(saver_image), "%s/%simage/screensaver.png", theme_base, mux_dim);

        if (!file_exist(saver_image)) {
            snprintf(saver_image, sizeof(saver_image), "%s/image/screensaver.png", theme_base);
            if (!file_exist(saver_image)) snprintf(saver_image, sizeof(saver_image), OPT_SHARE_PATH "media/logo.png");
        }

        dvd_init(monitor.renderer, saver_image, device.screen.width, device.screen.height);
    } else if (type == saver_type_slideshow) {
        char slide_dirs[4][MAX_BUFFER_SIZE];
        const char *slide_dir_ptrs[4];
        int slide_dir_count = 0;

        snprintf(slide_dirs[slide_dir_count], sizeof(slide_dirs[0]), "%s/%simage/slideshow", theme_base, mux_dim);
        slide_dir_ptrs[slide_dir_count] = slide_dirs[slide_dir_count];
        slide_dir_count++;

        snprintf(slide_dirs[slide_dir_count], sizeof(slide_dirs[0]), "%s/image/slideshow", theme_base);
        slide_dir_ptrs[slide_dir_count] = slide_dirs[slide_dir_count];
        slide_dir_count++;

        if (device.storage.sdcard.mount[0]) {
            snprintf(
                slide_dirs[slide_dir_count], sizeof(slide_dirs[0]), "%s/MUOS/slideshow", device.storage.sdcard.mount
            );
            slide_dir_ptrs[slide_dir_count] = slide_dirs[slide_dir_count];
            slide_dir_count++;
        }

        if (device.storage.rom.mount[0]) {
            snprintf(slide_dirs[slide_dir_count], sizeof(slide_dirs[0]), "%s/MUOS/slideshow", device.storage.rom.mount);
            slide_dir_ptrs[slide_dir_count] = slide_dirs[slide_dir_count];
            slide_dir_count++;
        }

        slideshow_init(monitor.renderer, slide_dir_ptrs, slide_dir_count, device.screen.width, device.screen.height);
    } else if (type == saver_type_boxart) {
        boxart_init(monitor.renderer, INFO_CAT_PATH, device.screen.width, device.screen.height);
    } else if (type == saver_type_bsod) {
        bsod_init(monitor.renderer, device.screen.width, device.screen.height);
    }

    active_saver = type;
}

void check_theme_change(void) {
    const char *theme = config.theme.active;
    if (strncmp(theme, monitor.theme_name, sizeof(monitor.theme_name)) != 0) {
        LOG_DEBUG("video", "Theme change detected: %s -> %s", monitor.theme_name, theme);

        if (video_wallpaper_active()) video_wallpaper_stop();
        reload_background(theme);
        reload_saver();

        monitor.refresh = 1;
    }
}

static int scale_pixels(const int px, const float zoom) {
    return (int) ((float) px * zoom + 0.5f);
}

static int pct_offset(const int screen, const int render, const float percent) {
    return (screen - render) / 2 + (int) (percent / 100.0f * (float) screen);
}

static void update_render_state(void) {
    switch (config.settings.general.theme_scaling) {
        case 0: // No Scaling
            scale_width = device.mux.width;
            scale_height = device.mux.height;
            LOG_INFO("video", "Scaling: Disabled");
            break;
        case 2: // Stretch to Screen
            if (device.screen.zoom_width <= 0.0f || device.screen.zoom_height <= 0.0f) {
                scale_width = device.mux.width;
                scale_height = device.mux.height;
            } else {
                scale_width = scale_pixels(device.mux.width, device.screen.zoom_width);
                scale_height = scale_pixels(device.mux.height, device.screen.zoom_height);
            }
            LOG_INFO("video", "Scaling: Stretch");
            break;
        default: // Scale with letterbox
            if (device.screen.zoom <= 0.0f) {
                scale_width = device.mux.width;
                scale_height = device.mux.height;
            } else {
                scale_width = scale_pixels(device.mux.width, device.screen.zoom);
                scale_height = scale_pixels(device.mux.height, device.screen.zoom);
            }
            LOG_INFO("video", "Scaling: Scale");
            break;
    }

    LOG_INFO("video", "Device Scale: %dx%d", scale_width, scale_height);

    underscan = config.boot.device_mode && config.settings.hdmi.scan ? 16 : 0;
    LOG_INFO("video", "Device Underscan: %d", underscan);

    const int offset_render_x = pct_offset(device.screen.width, scale_width, device.screen.render_offset_x);
    const int offset_render_y = pct_offset(device.screen.height, scale_height, device.screen.render_offset_y);

    const int offset_theme_x = pct_offset(scale_width, scale_width, theme.sdl.render.offset_x);
    const int offset_theme_y = pct_offset(scale_height, scale_height, theme.sdl.render.offset_y);

    monitor.dest_rect =
        (SDL_Rect) {offset_render_x + offset_theme_x + underscan, offset_render_y + offset_theme_y + underscan,
                    scale_width - underscan * 2, scale_height - underscan * 2};

    if (monitor.dest_rect.w <= 0 || monitor.dest_rect.h <= 0) {
        monitor.dest_rect = (SDL_Rect) {0, 0, device.screen.width, device.screen.height};
    }

    if (hdmi_mode) {
        monitor.angle = 0.0;
        monitor.pivot_ptr = NULL;
    } else {
        monitor.angle = device.screen.rotate <= 3 ? device.screen.rotate * 90.0 : device.screen.rotate;
        monitor.pivot = (SDL_Point) {device.screen.rotate_pivot_x, device.screen.rotate_pivot_y};
        monitor.pivot_ptr = monitor.pivot.x > 0 && monitor.pivot.y > 0 ? &monitor.pivot : NULL;
    }

    // So if we are not covering the whole window we'll clear it to avoid artifacts
    monitor.needs_clear = monitor.dest_rect.x > 0 || monitor.dest_rect.y > 0
                          || monitor.dest_rect.w < device.screen.width || monitor.dest_rect.h < device.screen.height
                          || underscan > 0;
}

void sdl_init(void) {
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_APP_NAME, MUX_CALLER);
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");

    if (hdmi_mode) {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    } else {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        LOG_ERROR("video", "SDL Init Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) LOG_ERROR("video", "PNG Init Failed: %s", IMG_GetError());

    SDL_ShowCursor(SDL_DISABLE);
    update_render_state();

    monitor.window = SDL_CreateWindow(
        MUX_CALLER, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, device.screen.width, device.screen.height,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!monitor.window) {
        LOG_ERROR("video", "Window Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    monitor.renderer = SDL_CreateRenderer(
        monitor.window, -1, hdmi_mode ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!monitor.renderer) {
        LOG_ERROR("video", "Renderer Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    int out_w = 0, out_h = 0;
    SDL_GetRendererOutputSize(monitor.renderer, &out_w, &out_h);

    monitor.texture = SDL_CreateTexture(
        monitor.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, device.mux.width, device.mux.height
    );
    if (!monitor.texture) {
        LOG_ERROR("video", "Texture Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_SetTextureScaleMode(monitor.texture, SDL_ScaleModeNearest);
    update_blend_mode();

    monitor.shadow_layer = SDL_CreateTexture(
        monitor.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, device.mux.width, device.mux.height
    );
    if (!monitor.shadow_layer) {
        LOG_ERROR("video", "Shadow layer texture creation failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_SetTextureBlendMode(monitor.shadow_layer, SDL_BLENDMODE_BLEND);

    /* Clear both render-target textures to transparent black */
    SDL_SetRenderTarget(monitor.renderer, monitor.shadow_layer);
    SDL_SetRenderDrawBlendMode(monitor.renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(monitor.renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(monitor.renderer, NULL);

    SDL_SetRenderTarget(monitor.renderer, monitor.texture);
    SDL_RenderFillRect(monitor.renderer, NULL);
    SDL_SetRenderDrawBlendMode(monitor.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(monitor.renderer, NULL);

    monitor.refresh = 1;
    monitor.force_clear = 1;

    LOG_INFO("video", "SDL Video Initialised Successfully");

    SDL_RendererInfo info;
    SDL_GetRendererInfo(monitor.renderer, &info);

    LOG_INFO("video", "SDL Renderer Method: %s (%dx%d)", info.name, out_w, out_h);
    LOG_INFO("video", "SDL Renderer Texture: %dx%d", info.max_texture_width, info.max_texture_height);

    reload_background(config.theme.active);
    reload_saver();
}

void sdl_cleanup(void) {
    anim_unload();
    if (active_saver == saver_type_video && video_saver_running) {
        video_wallpaper_stop();
        video_saver_running = 0;
    }
    dvd_shutdown();
    star_shutdown();
    matrix_shutdown();
    firefly_shutdown();
    pulse_shutdown();
    trace_shutdown();
    constellation_shutdown();
    mystify_shutdown();
    maze_shutdown();
    blockfall_shutdown();
    datetime_shutdown();
    slideshow_shutdown();
    boxart_shutdown();
    bsod_shutdown();

    if (monitor.shadow_layer) {
        SDL_DestroyTexture(monitor.shadow_layer);
        monitor.shadow_layer = NULL;
    }
    if (monitor.texture) SDL_DestroyTexture(monitor.texture);
    if (monitor.renderer) SDL_DestroyRenderer(monitor.renderer);
    if (monitor.window) SDL_DestroyWindow(monitor.window);
    if (monitor.background_image) SDL_DestroyTexture(monitor.background_image);
    if (monitor.theme_overlay) SDL_DestroyTexture(monitor.theme_overlay);

    IMG_Quit();
    SDL_Quit();
}

static void drain_saver_launch_input(void) {
    SDL_Event ev;
    const uint32_t start = SDL_GetTicks();

    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    while (SDL_GetTicks() - start < 180) {
        while (SDL_PollEvent(&ev)) {
        }
        SDL_Delay(10);
    }
}

static void render_saver_frame(void) {
    SDL_SetRenderTarget(monitor.renderer, NULL);
    SDL_SetRenderDrawColor(monitor.renderer, theme.sdl.solid.r, theme.sdl.solid.g, theme.sdl.solid.b, 255);
    SDL_RenderClear(monitor.renderer);

    saver_render(monitor.renderer);

    SDL_RenderPresent(monitor.renderer);
    SDL_SetRenderTarget(monitor.renderer, monitor.texture);
}

static int saver_event_is_touch(const SDL_Event *ev) {
    if (!device.board.has_touch) return 0;

    switch (ev->type) {
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
        case SDL_MULTIGESTURE:
        case SDL_DOLLARGESTURE:
        case SDL_DOLLARRECORD:
            return 1;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (ev->button.which == SDL_TOUCH_MOUSEID) return 1;
            return 1;
        case SDL_MOUSEMOTION:
            if (ev->motion.which == SDL_TOUCH_MOUSEID) return 1;
            return 1;
        case SDL_MOUSEWHEEL:
            if (ev->wheel.which == SDL_TOUCH_MOUSEID) return 1;
            return 1;
        default:
            return 0;
    }
}

static int saver_event_should_stop(const SDL_Event *ev) {
    if (saver_event_is_touch(ev)) return 0;

    switch (ev->type) {
        case SDL_KEYDOWN:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_JOYBUTTONDOWN:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_QUIT:
            return 1;
        case SDL_CONTROLLERAXISMOTION:
            return abs(ev->caxis.value) >= 16384;
        case SDL_JOYAXISMOTION:
            return abs(ev->jaxis.value) >= 16384;
        default:
            return 0;
    }
}

static void run_saver_loop(int preview) {
    mux_input_flush_all();

    if (preview) drain_saver_launch_input();

    SDL_Event ev;
    uint32_t next = SDL_GetTicks();
    uint32_t last_status = SDL_GetTicks();

    while (preview || saver_active()) {
        const uint32_t frame_ms = IDLE_MS;
        uint32_t now = SDL_GetTicks();
        const int timeout = (int) (next > now ? next - now : 0);

        if (SDL_WaitEventTimeout(&ev, timeout)) {
            do {
                if (saver_event_should_stop(&ev)) {
                    saver_stop();
                    preview = 0;
                    break;
                }
            } while (SDL_PollEvent(&ev));
        }

        if (!preview && !saver_active()) break;

        saver_update();

        if (!preview && !saver_active()) break;

        render_saver_frame();
        next += frame_ms;

        now = SDL_GetTicks();
        if (now - last_status >= TIMER_STATUS) {
            status_poll();
            last_status = now;
        }

        if (next <= now) next = now + frame_ms;
    }

    monitor.force_clear = 1;
    monitor.refresh = 1;

    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    mux_input_resume();
}

void preview_saver(const int type, const int speed) {
    if (!monitor.renderer || type == saver_type_disabled || speed <= 0) return;

    const int old_type = config.settings.power.saver_type;
    const int old_speed = config.settings.power.saver_speed;
    const int old_idle = read_line_int_from(IDLE_STATE, 0);

    saver_set_speed_override(speed);

    config.settings.power.saver_type = (int16_t) type;
    config.settings.power.saver_speed = (int16_t) speed;

    write_text_to_file(IDLE_STATE, "w", INT, 1);

    reload_saver();

    monitor.force_clear = 1;
    monitor.refresh = 1;

    for (int i = 0; i < 3; i++) {
        saver_update();
        render_saver_frame();
        SDL_Delay(IDLE_MS);
    }

    run_saver_loop(1);

    write_text_to_file(IDLE_STATE, "w", INT, old_idle);

    saver_clear_speed_override();

    config.settings.power.saver_type = (int16_t) old_type;
    config.settings.power.saver_speed = (int16_t) old_speed;

    reload_saver();

    last_saver_exit = SDL_GetTicks();

    monitor.force_clear = 1;
    monitor.refresh = 1;
}

void display_composite_frame(void) {
    if (!monitor.renderer || !monitor.texture) return;

    SDL_SetRenderTarget(monitor.renderer, NULL);

    const int animating = anim_is_active();
    const int anim_fg = animating && anim_is_foreground();
    const int anim_bg = animating && !anim_fg;

    if (monitor.background_image) {
        const SDL_Rect full = {0, 0, device.screen.width, device.screen.height};
        SDL_RenderCopy(monitor.renderer, monitor.background_image, NULL, &full);
    } else {
        SDL_SetRenderDrawColor(monitor.renderer, theme.sdl.solid.r, theme.sdl.solid.g, theme.sdl.solid.b, 255);
        SDL_RenderClear(monitor.renderer);
    }

    monitor.force_clear = 0;

    if (anim_bg) {
        if (!gradient_captured) {
            void *buf;
            int gw, gh;
            ui_common_get_gradient_buffer(&buf, &gw, &gh);
            if (buf) {
                SDL_Surface *s =
                    SDL_CreateRGBSurfaceFrom(buf, gw, gh, 32, gw * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
                if (s) {
                    anim_set_gradient(SDL_CreateTextureFromSurface(monitor.renderer, s));
                    SDL_FreeSurface(s);
                }
                gradient_captured = 1;
            }
        }
        anim_tick(monitor.renderer);
        SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_BLEND);
    } else if (video_bg_fn_ptr) {
        gradient_captured = 0;
        video_bg_fn_ptr(monitor.renderer);
        SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_BLEND);
    } else {
        gradient_captured = 0;
        SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_BLEND);
    }

    if (monitor.shadow_layer) {
        if (monitor.angle == 0.0) {
            SDL_RenderCopy(monitor.renderer, monitor.shadow_layer, NULL, &monitor.dest_rect);
        } else {
            SDL_RenderCopyEx(
                monitor.renderer, monitor.shadow_layer, NULL, &monitor.dest_rect, monitor.angle, monitor.pivot_ptr,
                SDL_FLIP_NONE
            );
        }
    }

    SDL_SetTextureBlendMode(
        monitor.texture, SDL_ComposeCustomBlendMode(
                             SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                             SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD
                         )
    );

    if (monitor.angle == 0.0) {
        SDL_RenderCopy(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect);
    } else {
        SDL_RenderCopyEx(
            monitor.renderer, monitor.texture, NULL, &monitor.dest_rect, monitor.angle, monitor.pivot_ptr, SDL_FLIP_NONE
        );
    }

    if (anim_fg) anim_tick(monitor.renderer);

    if (monitor.theme_overlay) {
        int tex_w, tex_h;
        SDL_QueryTexture(monitor.theme_overlay, NULL, NULL, &tex_w, &tex_h);
        const SDL_Rect dst = {(device.screen.width - tex_w) / 2, (device.screen.height - tex_h) / 2, tex_w, tex_h};
        SDL_SetTextureBlendMode(monitor.theme_overlay, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(monitor.theme_overlay, monitor.theme_overlay_opacity);
        SDL_RenderCopy(monitor.renderer, monitor.theme_overlay, NULL, &dst);
    }

    if (video_overlay_fn_ptr) video_overlay_fn_ptr(monitor.renderer);

    if (display_fade_alpha > 0) {
        SDL_SetRenderDrawBlendMode(monitor.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(monitor.renderer, 0, 0, 0, display_fade_alpha);
        SDL_RenderFillRect(monitor.renderer, NULL);
    }

    SDL_RenderPresent(monitor.renderer);
    SDL_SetRenderTarget(monitor.renderer, monitor.texture);
}

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    (void) area;
    (void) color_p;

    if (!monitor.renderer) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    if (!lv_disp_flush_is_last(disp_drv)) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    {
        static unsigned last_saver_type_seen = 0;
        static uint32_t last_type_poll = 0;
        const uint32_t now = SDL_GetTicks();

        int should_check;
        if (ino_proc) {
            should_check = saver_type_changes != last_saver_type_seen;
            if (should_check) last_saver_type_seen = saver_type_changes;
        } else {
            should_check = now - last_type_poll >= TIMER_IDLE;
            if (should_check) last_type_poll = now;
        }

        if (should_check) {
            int wanted_type = read_line_int_from(CONF_CONFIG_PATH "settings/power/saver_type", 1);
            if (wanted_type < 0) wanted_type = 0;

            if (active_saver != wanted_type) {
                config.settings.power.saver_type = (int16_t) wanted_type;
                reload_saver();
            }
        }
    }

    if (active_saver != saver_type_disabled) {
        const uint32_t now = SDL_GetTicks();

        if (now - last_saver_exit > SAVER_DELAY) {
            saver_update();
            if (saver_active()) {
                run_saver_loop(0);
                last_saver_exit = SDL_GetTicks();
            }
        }
    }

    display_composite_frame();
    lv_disp_flush_ready(disp_drv);
}

void canvas_invalidate_gpu_texture(lv_obj_t *canvas) {
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp || !disp->driver || !disp->driver->draw_ctx) return;

    lv_draw_sdl_ctx_t *sdl_ctx = (lv_draw_sdl_ctx_t *) disp->driver->draw_ctx;
    lv_draw_sdl_texture_cache_remove_src(sdl_ctx, lv_canvas_get_img(canvas));
}
