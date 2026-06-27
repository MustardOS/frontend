#pragma once

#include "../lvgl/lvgl.h"

void video_wallpaper_play(const char *path);

void video_wallpaper_stop(void);

int video_wallpaper_active(void);

void video_preview_arm(const char *path, int delay_ms, const lv_obj_t *container, const lv_obj_t *box_img);

int video_preview_active(void);

void video_preview_cancel(void);

void video_preview_destroy(void);
