#pragma once

#include "../../lvgl/lvgl.h"

void font_cache_clear(void);

int get_font_size(void);

void load_font_text(lv_obj_t *screen);

void load_font_section(const char *section, lv_obj_t *element);

lv_font_t *load_font_pass_roller(void);

int font_context_changed(void);

int theme_has_font(void);

int theme_font_is_scalable(void);

int user_font_path(const char *name, char *out, size_t out_size);

int user_font_count(void);
