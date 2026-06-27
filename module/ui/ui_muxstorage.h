#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxstorage(lv_obj_t *ui_pnl_content);

#define STORAGE(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_storage;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_storage;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_storage;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_storage;

STORAGE_ELEMENTS
#undef STORAGE
