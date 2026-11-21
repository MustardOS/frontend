#pragma once

#include <stddef.h>

typedef void (*list_text_fn)(
    int index, char *label, size_t label_size, char *value, size_t value_size, const char **glyph
);

void list_init(void);

void list_show(const char *title, int count, int selected, list_text_fn text);

void list_hide(void);

int list_is_visible(void);

int list_selected(void);

void list_move(int delta, int wrap);

void list_refresh(void);
