#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>
#include "../lvgl/lvgl.h"

void check_theme_change(void);

void sdl_init(void);

void sdl_cleanup(void);

void preview_saver(int type, int speed);

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

void display_check_idle_saver(void);

void display_set_idle_saver_suppressed_query(int (*fn)(void));

void display_set_fade_alpha(uint8_t alpha);

void display_composite_frame(void);

uint64_t display_present_serial(void);

void display_set_ui_hidden(int hidden);

int display_ui_is_hidden(void);

int display_panel_refresh_hz(void);

int display_video_fast_path_allowed(void);

int display_video_needs_logical_target(void);

void display_map_logical_rect(const SDL_Rect *logical, SDL_Rect *physical);

void display_render_logical_texture(SDL_Renderer *renderer, SDL_Texture *texture);

void display_set_composite_suppressed(int suppressed);

int display_capture_clean_frame(const char *path);

int display_capture_clean_pixels(uint8_t *rgb, int width, int height);

SDL_Renderer *display_get_renderer(void);

SDL_Window *display_get_window(void);

SDL_Texture *display_get_shadow_layer(void);

typedef void (*display_overlay_fn)(SDL_Renderer *r);

void display_set_video_overlay(display_overlay_fn fn);

void display_clear_video_overlay(void);

void display_set_video_background(display_overlay_fn fn);

void display_clear_video_background(void);

void display_set_video_background_opaque(int opaque);

void display_set_hard_sync_query(int (*fn)(void));

void display_set_present_timing(void (*fn)(double draw_ms, double flip_ms));

double display_take_flip_ms(void);

SDL_Texture *display_load_png_texture(const char *path);

void display_set_theme_overlay(SDL_Texture *tex, uint8_t opacity);

void display_update_overlay_opacity(uint8_t opacity);

void display_clear_theme_overlay(void);

void canvas_invalidate_gpu_texture(lv_obj_t *canvas);
