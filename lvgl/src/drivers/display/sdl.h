#pragma once

#include "../../../../lvgl/lvgl.h"
#include "../../../lv_drv_conf.h"

void sdl_init(void);

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
