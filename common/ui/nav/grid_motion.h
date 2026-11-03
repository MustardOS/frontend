#pragma once

#include <lvgl/lvgl.h>

void grid_motion_reset(void);

int grid_motion_owns(const lv_obj_t *obj);

int grid_motion_carousel_prepare(int count);

void grid_motion_carousel_step(int direction);

void grid_motion_pop_focused(void);
