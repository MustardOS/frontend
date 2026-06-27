#pragma once

#include <SDL2/SDL.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/lv_drv_conf.h"

void check_theme_change(void);

void sdl_init(void);

void sdl_cleanup(void);

int get_saver_speed(int fallback);

void preview_saver(int type, int speed);

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, const lv_color_t *color_p);

void display_set_fade_alpha(uint8_t alpha);

void display_composite_frame(void);

SDL_Renderer *display_get_renderer(void);

SDL_Texture *display_get_shadow_layer(void);

typedef void (*display_overlay_fn)(SDL_Renderer *r);

void display_set_video_overlay(display_overlay_fn fn);

void display_clear_video_overlay(void);

void display_set_video_background(display_overlay_fn fn);

void display_clear_video_background(void);

SDL_Texture *display_load_png_texture(const char *path);

void display_set_theme_overlay(SDL_Texture *tex, uint8_t opacity);

void display_update_overlay_opacity(uint8_t opacity);

void display_clear_theme_overlay(void);

void canvas_invalidate_gpu_texture(lv_obj_t *canvas);
