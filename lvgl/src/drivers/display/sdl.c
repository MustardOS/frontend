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
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    volatile bool refresh;
    uint32_t *pixel;
} monitor_t;

static monitor_t monitor;

int scale_width, scale_height, underscan;

void sdl_init(void) {
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_SetHint(SDL_HINT_APP_NAME, MUX_CALLER);

    SDL_Init(SDL_INIT_VIDEO);

    scale_width = device.MUX.WIDTH * device.SCREEN.ZOOM;
    scale_height = device.MUX.HEIGHT * device.SCREEN.ZOOM;
    LOG_INFO("video", "Device Scale: %dx%d", scale_width, scale_height)

    underscan = (config.BOOT.DEVICE_MODE && config.SETTINGS.HDMI.SCAN) ? 16 : 0;
    LOG_INFO("video", "Device Underscan: %d", underscan)

    monitor.pixel = calloc(device.MUX.WIDTH * device.MUX.HEIGHT, sizeof(uint32_t));
    if (!monitor.pixel) {
        LOG_ERROR("video", "Framebuffer Allocation Failure")
        exit(EXIT_FAILURE);
    }

    monitor.window = SDL_CreateWindow(MUX_CALLER, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      device.SCREEN.WIDTH, device.SCREEN.HEIGHT,
                                      SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!monitor.window) {
        LOG_ERROR("video", "Window Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    monitor.renderer = SDL_CreateRenderer(monitor.window, -1,
                                          SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!monitor.renderer) {
        LOG_ERROR("video", "Renderer Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    monitor.texture = SDL_CreateTexture(monitor.renderer,
                                        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                        device.MUX.WIDTH, device.MUX.HEIGHT);
    if (!monitor.texture) {
        LOG_ERROR("video", "Texture Creation Failed: %s", SDL_GetError())
        exit(EXIT_FAILURE);
    }

    SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(monitor.texture, NULL, monitor.pixel, device.MUX.WIDTH * sizeof(uint32_t));
    monitor.refresh = true;

    LOG_INFO("video", "SDL Video Initialised Successfully")
}

void sdl_cleanup(void) {
    if (monitor.texture) SDL_DestroyTexture(monitor.texture);
    if (monitor.renderer) SDL_DestroyRenderer(monitor.renderer);
    if (monitor.window) SDL_DestroyWindow(monitor.window);
    free(monitor.pixel);
    SDL_Quit();
}

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!monitor.pixel || area->x2 < 0 || area->y2 < 0 ||
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

    // Optimised full-screen flush...
    if (x1 == 0 && y1 == 0 && x2 == device.MUX.WIDTH - 1 && y2 == device.MUX.HEIGHT - 1) {
        memcpy(monitor.pixel, color_p, device.MUX.WIDTH * device.MUX.HEIGHT * sizeof(lv_color_t));
    } else {
        for (int y = y1; y <= y2; y++) {
            memcpy(&monitor.pixel[y * device.MUX.WIDTH + x1], color_p, (x2 - x1 + 1) * sizeof(lv_color_t));
            color_p += (x2 - x1 + 1);
        }
    }

    SDL_Rect update_rect = {x1, y1, x2 - x1 + 1, y2 - y1 + 1};
    SDL_UpdateTexture(monitor.texture, &update_rect,
                      &monitor.pixel[y1 * device.MUX.WIDTH + x1],
                      device.MUX.WIDTH * sizeof(uint32_t));

    // Flush the buffer only if it's the last frame however I may be reading
    // this wrong because it's always being flushed?! No clue...
    // Maybe somebody will notice this one day and do it better? :D
    if (lv_disp_flush_is_last(disp_drv)) {
        lv_disp_t *disp = _lv_refr_get_disp_refreshing();
        if (!disp || disp->driver->full_refresh) SDL_RenderClear(monitor.renderer);

        if (disp && disp->driver->screen_transp) {
            SDL_SetRenderDrawColor(monitor.renderer, 0xff, 0, 0, 0xff);
            SDL_Rect r = {0, 0, device.MUX.WIDTH, device.MUX.HEIGHT};
            SDL_RenderDrawRect(monitor.renderer, &r);
        }

        SDL_Rect dest_rect = {
                ((device.SCREEN.WIDTH - scale_width) / 2) + underscan,
                ((device.SCREEN.HEIGHT - scale_height) / 2) + underscan,
                scale_width - (underscan * 2),
                scale_height - (underscan * 2)
        };

        // Simply the rendering if we aren't rotating, saves a few cycles
        double angle = device.SCREEN.ROTATE <= 3 ? (device.SCREEN.ROTATE * 90.0) : device.SCREEN.ROTATE;
        if (angle == 0.0) {
            SDL_RenderCopy(monitor.renderer, monitor.texture, NULL, &dest_rect);
        } else {
            SDL_RenderCopyEx(monitor.renderer, monitor.texture, NULL, &dest_rect, angle, NULL, SDL_FLIP_NONE);
        }

        SDL_RenderPresent(monitor.renderer);
    }

    lv_disp_flush_ready(disp_drv);
}
