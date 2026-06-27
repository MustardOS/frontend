#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxnetadv(lv_obj_t *ui_pnl_content);

#define NETADV(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_netadv;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_netadv;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_netadv;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_netadv;

NETADV_ELEMENTS
#undef NETADV

extern int wait_retry_int[];

extern char *wait_retry_str[];
