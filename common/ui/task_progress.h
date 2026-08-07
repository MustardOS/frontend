#pragma once

#include "../../lvgl/lvgl.h"
#include "../task_exec.h"
#include "../theme.h"

void task_progress_init(struct theme_config *t, lv_obj_t *parent);

void task_progress_show(void);

void task_progress_hide(void);

int task_progress_active(void);

void task_progress_tick(void);

void task_progress_reset(void);

int task_progress_handle_a(void);

int task_progress_handle_b(void);

int task_progress_handle_dpad(int direction);

int task_progress_handle_dpad_hold(int direction);
