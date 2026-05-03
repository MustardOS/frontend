#pragma once

#include "../lvgl/lvgl.h"
#include "../lvgl/lv_drv_conf.h"

void check_theme_change(void);

void sdl_init(void);

void sdl_cleanup(void);

int get_saver_speed(int fallback);

void preview_saver(int type, int speed);

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
