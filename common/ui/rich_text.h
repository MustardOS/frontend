#pragma once

#include <lvgl/lvgl.h>

lv_obj_t *rich_text_create(lv_obj_t *parent, const char *markup, const lv_font_t *base_font);

void rich_text_set(lv_obj_t *group, const char *markup, const lv_font_t *base_font);
