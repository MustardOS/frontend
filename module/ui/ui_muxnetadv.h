#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxnetadv(lv_obj_t *ui_pnlContent);

#define NETADV(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_netadv; \
    extern lv_obj_t *ui_lbl##NAME##_netadv; \
    extern lv_obj_t *ui_ico##NAME##_netadv; \
    extern lv_obj_t *ui_dro##NAME##_netadv;

NETADV_ELEMENTS
#undef NETADV

extern const int wait_retry_int[];
