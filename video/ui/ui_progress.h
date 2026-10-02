#pragma once

#include <lvgl/lvgl.h>

#define WASABI_PROGRESS_PART_CAPACITY 48

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *back;
    lv_obj_t *primary;
    lv_obj_t *secondary;
    lv_obj_t *marker;
    lv_obj_t *parts[WASABI_PROGRESS_PART_CAPACITY];
    int part_count;
    int width;
    int shown;
    int shown_step;
    int style;
} wasabi_progress;

void wasabi_progress_init(wasabi_progress *progress, lv_obj_t *parent, int width);
int wasabi_progress_update(wasabi_progress *progress, int value);
void wasabi_progress_reset(wasabi_progress *progress);
