#pragma once

#include "../../lvgl/lvgl.h"
#include "../task_exec.h"
#include "../theme.h"
#include "dialogue.h"

#define TASK_PROMPT_MAX_OPTIONS TASK_OPTION_MAX

void task_prompt_init(struct theme_config *t, lv_obj_t *parent);

int task_prompt_active(void);

int task_prompt_show(const task_event *prompt);

void task_prompt_hide(void);

int task_prompt_handle_a(void);

int task_prompt_handle_b(void);

int task_prompt_handle_dpad(int direction);

int task_prompt_handle_dpad_hold(int direction);
