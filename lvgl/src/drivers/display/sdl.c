#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../../../common/device.h"
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
    const lv_coord_t h_res = disp_drv->physical_hor_res == -1 ? disp_drv->hor_res : disp_drv->physical_hor_res;
    const lv_coord_t v_res = disp_drv->physical_ver_res == -1 ? disp_drv->ver_res : disp_drv->physical_ver_res;

    if (area->x2 < 0 || area->y2 < 0 || area->x1 > h_res - 1 || area->y1 > v_res - 1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    monitor.pixel = (uint32_t *) color_p;
    monitor.refresh = true;

    if (lv_disp_flush_is_last(disp_drv)) display_refresh();
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
                                   device.SCREEN.WIDTH, device.SCREEN.HEIGHT);

    SDL_SetTextureBlendMode(m->texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(m->texture, NULL, m->pixel, device.SCREEN.WIDTH * sizeof(uint32_t));

    m->refresh = true;
}

static void display_update(monitor_t *m) {
    if (m->pixel == NULL) return;

    SDL_UpdateTexture(m->texture, NULL, m->pixel, device.SCREEN.WIDTH * sizeof(uint32_t));
    SDL_RenderClear(m->renderer);

    lv_disp_t *d = _lv_refr_get_disp_refreshing();

    if (d->driver->screen_transp) {
        SDL_SetRenderDrawColor(m->renderer, 0xff, 0, 0, 0xff);
        SDL_Rect r = {0, 0, device.SCREEN.WIDTH, device.SCREEN.HEIGHT};
        SDL_RenderDrawRect(m->renderer, &r);
    }
    int scale_width = (device.SCREEN.WIDTH * device.SCREEN.ZOOM);
    int scale_height = (device.SCREEN.HEIGHT * device.SCREEN.ZOOM);

    SDL_SetWindowSize(m->window, scale_width, scale_height);

    int display_w, display_h;
    SDL_GetWindowSize(m->window, &display_w, &display_h);
    SDL_SetWindowPosition(m->window, (display_w - scale_width) / 2, (display_h - scale_height) / 2);

    SDL_Rect dest_rect = {
            (device.SCREEN.WIDTH - scale_width) / 2,
            (device.SCREEN.HEIGHT - scale_height) / 2,
            scale_width,
            scale_height
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

