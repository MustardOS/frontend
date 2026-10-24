#pragma once

#include <SDL2/SDL.h>
#include <lvgl/lvgl.h>

void video_wallpaper_play(const char *path);

void video_wallpaper_stop(void);

int video_wallpaper_active(void);

typedef int (*video_size_cb)(int width, int height, int *target_w, int *target_h);

void video_preview_arm(const char *path, int delay_ms, const lv_obj_t *container, const lv_obj_t *box_img);

void video_preview_arm_image(const char *path, lv_obj_t *image, video_size_cb size);

void video_preview_arm_image_delay(const char *path, lv_obj_t *image, video_size_cb size, int delay_ms);

int video_preview_matches(const char *path);

int video_preview_active(void);

void video_preview_cancel(void);

void video_preview_destroy(void);

void video_wallpaper_render_frame(SDL_Renderer *r);

const char *video_wallpaper_path(void);
