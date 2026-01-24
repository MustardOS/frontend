#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdevice(lv_obj_t *ui_pnlContent);

#define DEVICE(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_device; \
    extern lv_obj_t *ui_lbl##NAME##_device; \
    extern lv_obj_t *ui_ico##NAME##_device; \
    extern lv_obj_t *ui_dro##NAME##_device;

DEVICE_ELEMENTS
#undef DEVICE
