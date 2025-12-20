#pragma once

#include "../lvgl/lvgl.h"

void font_cache_clear(void);

void load_font_text(lv_obj_t *screen);

void load_font_section(const char *section, lv_obj_t *element);

int font_context_changed(void);
