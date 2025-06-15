
#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxbackup(lv_obj_t *ui_pnlContent);

#define BACKUP(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_backup; \
    extern lv_obj_t *ui_lbl##NAME##_backup; \
    extern lv_obj_t *ui_ico##NAME##_backup; \
    extern lv_obj_t *ui_lbl##NAME##Value_backup;

BACKUP_ELEMENTS
#undef BACKUP
