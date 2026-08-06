#pragma once

#include "../../lvgl/lvgl.h"
#include "../device.h"
#include "../theme.h"

void empty_state_init(struct theme_config *t, struct mux_device *d, lv_obj_t *parent);

void empty_state_show(const char *headline, const char *guidance);

void empty_state_show_action(const char *headline, const char *guidance, const char *glyph, const char *label);

void empty_state_hide(void);
