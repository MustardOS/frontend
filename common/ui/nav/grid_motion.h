#pragma once

#include <lvgl/lvgl.h>

void grid_motion_reset(void);

int grid_motion_owns(const lv_obj_t *obj);

int grid_motion_carousel_prepare(int count);

void grid_motion_carousel_step(int direction);

void grid_motion_carousel_ghost(lv_obj_t *ghost, int direction);

void grid_motion_swell_focused(void);
