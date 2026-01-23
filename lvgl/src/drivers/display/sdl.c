#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../../../../common/log.h"
#include "../../../../common/options.h"
#include "../../../../common/common.h"
#include "../../../../common/device.h"
#include "../../../../common/config.h"
#include "../../../../common/theme.h"
#include "sdl.h"
#include "dvd.h"

typedef struct {
    // Just the default base stuff we need
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    volatile bool refresh;

    // Cached render state
    SDL_Rect dest_rect;
    double angle;
    SDL_Point pivot;
    SDL_Point *pivot_ptr;

    // Track if we should clear
    bool needs_clear;
    bool force_clear;

    // Background image support
    bool background_solid;
    SDL_Texture *background_image;
    SDL_Color background_colour;
    char theme_name[256];
} monitor_t;

static monitor_t monitor;

int scale_width, scale_height, underscan;
int hdmi_in_use = 0;

static void update_blend_mode(void) {
    SDL_SetTextureBlendMode(monitor.texture, theme.SDL.TEXTURE_BLEND_MODE);
    SDL_SetRenderDrawBlendMode(monitor.renderer, theme.SDL.DRAW_BLEND_MODE);

    monitor.force_clear = true;
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

static void reload_background(const char *active_theme) {
    char back_image[MAX_BUFFER_SIZE];
    snprintf(back_image, sizeof(back_image), "%s/%simage/background.png", theme_base, mux_dimension);

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
            monitor.background_solid = true;
            monitor.background_colour = (SDL_Color) {theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255};
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

    strncpy(monitor.theme_name, active_theme, sizeof(monitor.theme_name) - 1);
    monitor.theme_name[sizeof(monitor.theme_name) - 1] = '\0';

    LOG_INFO("video", "Loaded theme background: %s", back_image);
}

static void reload_screensaver() {
    dvd_shutdown();

    char saver_image[MAX_BUFFER_SIZE];
    snprintf(saver_image, sizeof(saver_image), "%s/%simage/screensaver.png", theme_base, mux_dimension);

    if (!file_exist(saver_image)) {
        snprintf(saver_image, sizeof(saver_image), "%s/image/screensaver.png", theme_base);
        if (!file_exist(saver_image)) snprintf(saver_image, sizeof(saver_image), OPT_SHARE_PATH "media/logo.png");
    }

    dvd_init(monitor.renderer, saver_image, device.SCREEN.WIDTH, device.SCREEN.HEIGHT);
}

void check_theme_change(void) {
    const char *theme = config.THEME.ACTIVE;
    if (strncmp(theme, monitor.theme_name, sizeof(monitor.theme_name)) != 0) {
        LOG_DEBUG("video", "Theme change detected: %s -> %s", monitor.theme_name, theme);

        reload_background(theme);
        reload_screensaver();

        monitor.refresh = true;
    }
}

static inline int scale_pixels(int px, float zoom) {
    return (int) ((float) px * zoom + 0.5f);
}

static inline int pct_offset(int screen, int render, float percent) {
    return ((screen - render) / 2) + (int) ((percent / 100.0f) * (float) screen);
}

static void update_render_state(void) {
    LOG_INFO("video", "Device Scale: %dx%d", scale_width, scale_height);

    switch (config.SETTINGS.GENERAL.THEME_SCALING) {
        case 0: // No Scaling
            scale_width = device.MUX.WIDTH;
            scale_height = device.MUX.HEIGHT;
            LOG_INFO("video", "Scaling: Disabled");
            break;
        case 2: // Stretch to Screen
            scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM_WIDTH);
            scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM_HEIGHT);
            LOG_INFO("video", "Scaling: Stretch");
            break;
        default: // Scale with letterbox
            scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM);
            scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM);
            LOG_INFO("video", "Scaling: Scale");
            break;
    }

    underscan = (config.BOOT.DEVICE_MODE && config.SETTINGS.HDMI.SCAN) ? 16 : 0;
    LOG_INFO("video", "Device Underscan: %d", underscan);

    monitor.dest_rect = (SDL_Rect) {
            pct_offset(device.SCREEN.WIDTH, scale_width, theme.SDL.RENDER.OFFSET_X) + underscan,
            pct_offset(device.SCREEN.HEIGHT, scale_height, theme.SDL.RENDER.OFFSET_Y) + underscan,

            scale_width - (underscan * 2),
            scale_height - (underscan * 2)
    };

    if (hdmi_in_use) {
        monitor.angle = 0.0;
        monitor.pivot_ptr = NULL;
    } else {
        monitor.angle = (device.SCREEN.ROTATE <= 3) ? device.SCREEN.ROTATE * 90.0 : device.SCREEN.ROTATE;
        monitor.pivot = (SDL_Point) {device.SCREEN.ROTATE_PIVOT_X, device.SCREEN.ROTATE_PIVOT_Y};
        monitor.pivot_ptr = (monitor.pivot.x > 0 && monitor.pivot.y > 0) ? &monitor.pivot : NULL;
    }

    // So if we are not covering the whole window we'll clear it to avoid artifacts
    monitor.needs_clear =
            (monitor.dest_rect.x > 0) ||
            (monitor.dest_rect.y > 0) ||
            (monitor.dest_rect.w < device.SCREEN.WIDTH) ||
            (monitor.dest_rect.h < device.SCREEN.HEIGHT) ||
            (underscan > 0);
}

void sdl_init(void) {
    hdmi_in_use = file_exist("/tmp/hdmi_in_use");
    LOG_INFO("video", "HDMI in use: %s", hdmi_in_use ? "yes" : "no");

    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_SetHint(SDL_HINT_APP_NAME, MUX_CALLER);
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

    if (hdmi_in_use) {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    } else {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("video", "SDL Init Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        LOG_ERROR("video", "PNG Init Failed: %s", IMG_GetError());
    }

    SDL_ShowCursor(SDL_DISABLE);
    update_render_state();

    monitor.window = SDL_CreateWindow(MUX_CALLER, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      device.SCREEN.WIDTH, device.SCREEN.HEIGHT,
                                      SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!monitor.window) {
        LOG_ERROR("video", "Window Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    monitor.renderer = SDL_CreateRenderer(monitor.window, -1,
                                          hdmi_in_use ? SDL_RENDERER_ACCELERATED
                                                      : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
    );

    if (!monitor.renderer) {
        LOG_ERROR("video", "Renderer Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    monitor.texture = SDL_CreateTexture(monitor.renderer,
                                        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                        device.MUX.WIDTH, device.MUX.HEIGHT);

    if (!monitor.texture) {
        LOG_ERROR("video", "Texture Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    update_blend_mode();

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(monitor.texture, NULL, &pixels, &pitch) == 0) {
        uint32_t * px = pixels;
        size_t pt = (pitch / 4) * device.MUX.HEIGHT;
        for (size_t i = 0; i < pt; i++) px[i] = 0x00000000;
        SDL_UnlockTexture(monitor.texture);
    }

    monitor.refresh = true;
    LOG_INFO("video", "SDL Video Initialised Successfully");

    int out_w = 0, out_h = 0;
    SDL_RendererInfo info;
    SDL_GetRendererInfo(monitor.renderer, &info);
    SDL_GetRendererOutputSize(monitor.renderer, &out_w, &out_h);
    LOG_INFO("video", "SDL Renderer: %s (%dx%d)", info.name, out_w, out_h);

    reload_background(config.THEME.ACTIVE);
    reload_screensaver();
}

void sdl_cleanup(void) {
    dvd_shutdown();

    if (monitor.texture) SDL_DestroyTexture(monitor.texture);
    if (monitor.renderer) SDL_DestroyRenderer(monitor.renderer);
    if (monitor.window) SDL_DestroyWindow(monitor.window);
    if (monitor.background_image) SDL_DestroyTexture(monitor.background_image);

    IMG_Quit();
    SDL_Quit();
}

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!monitor.texture || area->x2 < 0 || area->y2 < 0 ||
        area->x1 >= device.MUX.WIDTH || area->y1 >= device.MUX.HEIGHT) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Since we've got direct mode switched off we need to clamp to the screen
    // or we'll get lovely glitches galore all around the place
    int x1 = LV_CLAMP(0, area->x1, device.MUX.WIDTH - 1);
    int y1 = LV_CLAMP(0, area->y1, device.MUX.HEIGHT - 1);
    int x2 = LV_CLAMP(0, area->x2, device.MUX.WIDTH - 1);
    int y2 = LV_CLAMP(0, area->y2, device.MUX.HEIGHT - 1);

    if (x2 < x1 || y2 < y1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Optimised full-screen flush...
    int copy_w = x2 - x1 + 1;
    int copy_h = y2 - y1 + 1;

    int src_w = area->x2 - area->x1 + 1;
    int src_x_ofs = x1 - area->x1;
    int src_y_ofs = y1 - area->y1;

    SDL_Rect upd = {x1, y1, copy_w, copy_h};

    void *dst_pixels = NULL;
    int dst_pitch = 0;

    if (SDL_LockTexture(monitor.texture, &upd, &dst_pixels, &dst_pitch) != 0) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Flush the buffer only if it's the last frame
    uint8_t *dst_row = (uint8_t *) dst_pixels;
    uint8_t *src_row = (uint8_t *) color_p + ((size_t) (src_y_ofs * src_w + src_x_ofs) * sizeof(lv_color_t));

    size_t row_bytes = (size_t) copy_w * sizeof(lv_color_t);
    size_t src_pitch = (size_t) src_w * sizeof(lv_color_t);

    for (int y = 0; y < copy_h; y++) {
        memcpy(dst_row, src_row, row_bytes);
        dst_row += dst_pitch;
        src_row += src_pitch;
    }

    SDL_UnlockTexture(monitor.texture);

    if (!lv_disp_flush_is_last(disp_drv)) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    if (config.SETTINGS.POWER.SCREENSAVER) dvd_update();

    // LVGL does not redraw at a steady pace when idle, so
    // the screensaver must become the "driver" while active...
    // I'm sure there are repercussions with this whole section
    // but we'll see what happens in the future!
    if (config.SETTINGS.POWER.SCREENSAVER && dvd_active()) {
        lv_disp_flush_ready(disp_drv);

        const uint32_t frame_ms = IDLE_MS;
        uint32_t next = SDL_GetTicks();

        while (1) {
            dvd_update();

            if (!dvd_active()) break;

            SDL_SetRenderDrawColor(monitor.renderer, theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255);
            SDL_RenderClear(monitor.renderer);

            dvd_render(monitor.renderer);
            SDL_RenderPresent(monitor.renderer);

            next += frame_ms;
            uint32_t now = SDL_GetTicks();

            if ((int32_t) (next - now) > 0) {
                SDL_Delay(next - now);
            } else {
                next = now;
            }
        }

        monitor.force_clear = true;
        monitor.refresh = true;

        return;
    }

    if (monitor.needs_clear || monitor.force_clear) {
        if (monitor.background_image) {
            SDL_Rect full = {0, 0, device.SCREEN.WIDTH, device.SCREEN.HEIGHT};
            SDL_RenderCopy(monitor.renderer, monitor.background_image, NULL, &full);
        } else {
            SDL_SetRenderDrawColor(monitor.renderer, theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255);
            SDL_RenderClear(monitor.renderer);
        }

        monitor.force_clear = false;
    }

    // LOG_DEBUG("sdl", "\tdest_rect: %d %d %d %d", dest_rect.x, dest_rect.y, dest_rect.w, dest_rect.h);

    // Simplify the rendering if we are not rotating as this saves a few cycles
    if (monitor.angle == 0.0) {
        SDL_RenderCopy(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect);
    } else {
        SDL_RenderCopyEx(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect,
                         monitor.angle, monitor.pivot_ptr, SDL_FLIP_NONE);
    }

    SDL_RenderPresent(monitor.renderer);
    lv_disp_flush_ready(disp_drv);
}
