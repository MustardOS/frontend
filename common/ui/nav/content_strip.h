#pragma once

#include <common/content/collection/common.h>

void content_strip_begin(void);

void content_strip_clear(void);

void content_strip_end(void);

void content_strip_sync(void);

int content_strip_split(const char *module, int *index);

int content_strip_shown(void);

int content_strip_active(void);

int content_strip_height(void);

int content_strip_position(int *count);

void content_strip_focus(int active);

int content_strip_move(int direction);

int content_strip_nav(int vertical, int direction);

const content_item *content_strip_selected(void);

int content_strip_saved_index(int list_index);
