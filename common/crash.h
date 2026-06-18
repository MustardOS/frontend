#pragma once

#include "ui/dialogue.h"
#include "language.h"

void crash_init(const char *module_name);

void crash_ui_check(struct theme_config *t, struct mux_lang *l, lv_obj_t *layer, int *msgbox_active);

void crash_ui_apply_font(lv_obj_t *source);

int crash_ui_dismiss(void);
