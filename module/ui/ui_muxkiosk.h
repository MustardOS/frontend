#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxkiosk(lv_obj_t *ui_pnlContent);

#define KIOSK(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_kiosk; \
    extern lv_obj_t *ui_lbl##NAME##_kiosk; \
    extern lv_obj_t *ui_ico##NAME##_kiosk; \
    extern lv_obj_t *ui_dro##NAME##_kiosk;

KIOSK_ELEMENTS
#undef KIOSK
