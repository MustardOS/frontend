#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxstorage(lv_obj_t *ui_pnlContent);

#define STORAGE(NAME, UDATA)                      \
    extern lv_obj_t *ui_pnl##NAME##_storage;      \
    extern lv_obj_t *ui_lbl##NAME##_storage;      \
    extern lv_obj_t *ui_ico##NAME##_storage;      \
    extern lv_obj_t *ui_lbl##NAME##Value_storage;

STORAGE_ELEMENTS
#undef STORAGE
