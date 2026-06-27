#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxbackup(lv_obj_t *ui_pnl_content);

#define BACKUP(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_backup;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_backup;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_backup;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_backup;

BACKUP_ELEMENTS
#undef BACKUP
