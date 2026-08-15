#pragma once

#include "../../lvgl/lvgl.h"
#include "../theme.h"

#define LIST_FRAME_MAX 16

typedef struct {
    const char *label;
    int first;
    int count;
} list_frame;

int list_frame_init(
    struct theme_config *t, lv_obj_t *parent, const list_frame *frames, int frame_count, lv_obj_t **panels,
    lv_obj_t **labels, lv_obj_t **glyphs, lv_obj_t **values, int total_rows
);

int list_frame_active(void);

int list_frame_focused(void);

void list_frame_help(void);

void list_frame_remember(const lv_obj_t *label);

void list_frame_remember_section(void);

int list_frame_current_row(void);

int list_frame_current(void);

void list_frame_set_inert(int row, int inert);

int list_frame_row_of(const lv_obj_t *label);

int list_frame_restore(void);

void list_frame_apply(void);

int list_frame_move(int direction);

int list_frame_go(int index);

void list_frame_reset(void);
