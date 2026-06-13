#pragma once

#include "../lvgl/lvgl.h"

extern int g_font_shadow_enabled;
extern lv_color_t g_shadow_colour_default;
extern lv_opa_t g_shadow_alpha_default;
extern int16_t g_shadow_x_offset_default;
extern int16_t g_shadow_y_offset_default;
extern lv_color_t g_shadow_colour_focus;
extern lv_opa_t g_shadow_alpha_focus;
extern int16_t g_shadow_x_offset_focus;
extern int16_t g_shadow_y_offset_focus;

void font_cache_clear(void);

void load_font_text(lv_obj_t *screen);

void load_font_section(const char *section, lv_obj_t *element);

lv_font_t *load_font_pass_roller(void);

int font_context_changed(void);

int theme_has_font(void);
