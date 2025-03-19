#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
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

static void display_create(monitor_t *m);

static void display_update(monitor_t *m);

static void display_refresh();

monitor_t monitor;

void sdl_init(void) {
    SDL_Init(SDL_INIT_VIDEO);
    display_create(&monitor);
}

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (area->x2 < 0 || area->y2 < 0 || area->x1 > device.MUX.WIDTH - 1 || area->y1 > device.MUX.HEIGHT - 1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    static lv_color_t *t_buf0 = NULL;
    static lv_color_t *t_buf1 = NULL;
    static lv_color_t *t_buf2 = NULL;
    static int buffers_initialized = 0;
    if (!buffers_initialized) {
        t_buf0 = malloc(device.MUX.WIDTH * device.MUX.HEIGHT * sizeof(lv_color_t));
        t_buf1 = malloc(device.MUX.WIDTH * device.MUX.HEIGHT * sizeof(lv_color_t));
        t_buf2 = malloc(device.MUX.WIDTH * device.MUX.HEIGHT * sizeof(lv_color_t));
        buffers_initialized = 1;
    }

    static lv_color_t *t_buf[3] = {NULL, NULL, NULL};
    if (t_buf[0] == NULL) {
        t_buf[0] = t_buf0;
        t_buf[1] = t_buf1;
        t_buf[2] = t_buf2;
    }

    static int t_buf_index = 0;

    lv_color_t *dest = &t_buf[t_buf_index][area->y1 * device.MUX.WIDTH + area->x1];
    memcpy(dest, color_p, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(lv_color_t));

    monitor.pixel = (uint32_t *) t_buf[t_buf_index];
    monitor.refresh = true;

    if (lv_disp_flush_is_last(disp_drv)) display_refresh();
    t_buf_index = (t_buf_index + 1) % 3;
    lv_disp_flush_ready(disp_drv);
}

static void display_refresh() {
    if (monitor.refresh != false) {
        monitor.refresh = false;
        display_update(&monitor);
    }
}

static void display_create(monitor_t *m) {
    m->window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 device.SCREEN.WIDTH, device.SCREEN.HEIGHT, SDL_WINDOW_FULLSCREEN);
    m->renderer = SDL_CreateRenderer(m->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    m->texture = SDL_CreateTexture(m->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                   device.MUX.WIDTH, device.MUX.HEIGHT);

    SDL_SetTextureBlendMode(m->texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(m->texture, NULL, m->pixel, device.MUX.WIDTH * sizeof(uint32_t));

    m->refresh = true;
}

static void display_update(monitor_t *m) {
    if (m->pixel == NULL) return;
    SDL_UpdateTexture(m->texture, NULL, m->pixel, device.MUX.WIDTH * sizeof(uint32_t));
    SDL_RenderClear(m->renderer);

    lv_disp_t *d = _lv_refr_get_disp_refreshing();

    if (d->driver->screen_transp) {
        SDL_SetRenderDrawColor(m->renderer, 0xff, 0, 0, 0xff);
        SDL_Rect r = {0, 0, device.MUX.WIDTH, device.MUX.HEIGHT};
        SDL_RenderDrawRect(m->renderer, &r);
    }
    int scale_width = (device.MUX.WIDTH * device.SCREEN.ZOOM);
    int scale_height = (device.MUX.HEIGHT * device.SCREEN.ZOOM);

    int underscan = read_int_from_file(RUN_GLOBAL_PATH "boot/device_mode", 1) && config.SETTINGS.HDMI.SCAN == 1 ? 16 : 0;

    SDL_Rect dest_rect = {
            ((device.SCREEN.WIDTH - scale_width) / 2) + underscan,
            ((device.SCREEN.HEIGHT - scale_height) / 2) + underscan,
            scale_width - (underscan *2),
            scale_height - (underscan *2)
    };

    double angle;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    switch (device.SCREEN.ROTATE) {
        case 0:
            angle = 0;
            break;
        case 1:
            angle = 90;
            break;
        case 2:
            angle = 180;
            break;
        case 3:
            angle = 270;
            break;
        default:
            angle = device.SCREEN.ROTATE;
            break;
    }

    SDL_RenderCopyEx(m->renderer, m->texture, NULL, &dest_rect, angle, NULL, flip);
    SDL_RenderPresent(m->renderer);
}
