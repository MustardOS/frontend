#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../../../common/log.h"
#include "../../../../common/options.h"
#include "../../../../common/common.h"
#include "../../../../common/device.h"
#include "../../../../common/config.h"
#include "sdl.h"

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
} monitor_t;

static monitor_t monitor;

int scale_width, scale_height, underscan;
int hdmi_in_use = 0;

static inline int scale_pixels(int px, float zoom) {
    return (int) ((float) px * zoom + 0.5f);
}

static void update_render_state(void) {
    LOG_INFO("video", "Device Scale: %dx%d", scale_width, scale_height)

    switch (config.SETTINGS.GENERAL.THEME_SCALING) {
        case 0: // No Scaling
            scale_width = device.MUX.WIDTH;
            scale_height = device.MUX.HEIGHT;
            LOG_INFO("video", "Scaling: Disabled")
            break;
        case 2: // Stretch to Screen
            scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM_WIDTH);
            scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM_HEIGHT);
            LOG_INFO("video", "Scaling: Stretch")
            break;
        default: // Scale with letterbox
            scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM);
            scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM);
            LOG_INFO("video", "Scaling: Scale")
            break;
    }

    underscan = (config.BOOT.DEVICE_MODE && config.SETTINGS.HDMI.SCAN) ? 16 : 0;
    LOG_INFO("video", "Device Underscan: %d", underscan)

    monitor.dest_rect = (SDL_Rect) {
            ((device.SCREEN.WIDTH - scale_width) / 2) + underscan,
            ((device.SCREEN.HEIGHT - scale_height) / 2) + underscan,
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
        LOG_ERROR("video", "SDL Init Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    SDL_ShowCursor(SDL_DISABLE);
    update_render_state();

    monitor.window = SDL_CreateWindow(MUX_CALLER, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      device.SCREEN.WIDTH, device.SCREEN.HEIGHT,
                                      SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!monitor.window) {
        LOG_ERROR("video", "Window Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    monitor.renderer = SDL_CreateRenderer(monitor.window, -1,
                                          hdmi_in_use ? SDL_RENDERER_ACCELERATED
                                                      : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
    );

    if (!monitor.renderer) {
        LOG_ERROR("video", "Renderer Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    monitor.texture = SDL_CreateTexture(monitor.renderer,
                                        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                        device.MUX.WIDTH, device.MUX.HEIGHT);

    if (!monitor.texture) {
        LOG_ERROR("video", "Texture Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_NONE);

    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(monitor.texture, NULL, &pixels, &pitch) == 0) {
        memset(pixels, 0, (size_t) pitch * (size_t) device.MUX.HEIGHT);
        SDL_UnlockTexture(monitor.texture);
    }

    monitor.refresh = true;
    LOG_INFO("video", "SDL Video Initialised Successfully")

    int out_w = 0, out_h = 0;
    SDL_RendererInfo info;
    SDL_GetRendererInfo(monitor.renderer, &info);
    SDL_GetRendererOutputSize(monitor.renderer, &out_w, &out_h);
    LOG_INFO("video", "SDL Renderer: %s (%dx%d)", info.name, out_w, out_h);
}

void sdl_cleanup(void) {
    if (monitor.texture) SDL_DestroyTexture(monitor.texture);
    if (monitor.renderer) SDL_DestroyRenderer(monitor.renderer);
    if (monitor.window) SDL_DestroyWindow(monitor.window);

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

    if (lv_disp_flush_is_last(disp_drv)) {
        if (monitor.needs_clear) SDL_RenderClear(monitor.renderer);

        // LOG_DEBUG("sdl", "\tdest_rect: %d %d %d %d", dest_rect.x, dest_rect.y, dest_rect.w, dest_rect.h)

        // Simplify the rendering if we are not rotating as this saves a few cycles
        if (monitor.angle == 0.0) {
            SDL_RenderCopy(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect);
        } else {
            SDL_RenderCopyEx(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect,
                             monitor.angle, monitor.pivot_ptr, SDL_FLIP_NONE);
        }

        SDL_RenderPresent(monitor.renderer);
    }

    lv_disp_flush_ready(disp_drv);
}
