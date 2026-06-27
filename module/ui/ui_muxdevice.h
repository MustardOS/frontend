#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdevice(lv_obj_t *ui_pnl_content);

#define DEVICE(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_device;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_device;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_device;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_device;

DEVICE_ELEMENTS
#undef DEVICE
