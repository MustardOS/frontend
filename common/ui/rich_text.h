#pragma once

#include <stddef.h>
#include <lvgl/lvgl.h>

#define RICH_TEXT_SIGN      "\xC2\xA7"
#define RICH_TEXT_BOLD      RICH_TEXT_SIGN "l"
#define RICH_TEXT_ITALIC    RICH_TEXT_SIGN "o"
#define RICH_TEXT_UNDERLINE RICH_TEXT_SIGN "n"
#define RICH_TEXT_STRIKE    RICH_TEXT_SIGN "m"
#define RICH_TEXT_RESET     RICH_TEXT_SIGN "r"

int rich_text_has_codes(const char *text);

void rich_text_strip(const char *text, char *out, size_t out_size);

lv_obj_t *rich_text_create(lv_obj_t *parent, const char *markup, const lv_font_t *base_font);

void rich_text_set(lv_obj_t *group, const char *markup, const lv_font_t *base_font);

void rich_text_bold_labels(const char *plain, char *out, size_t out_size);

void rich_text_label_set(lv_obj_t *label, const char *text);
