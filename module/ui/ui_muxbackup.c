#include "ui_muxshare.h"
#include "ui_muxbackup.h"

#define BACKUP(NAME, UDATA)               \
    lv_obj_t *ui_pnl##NAME##_backup;      \
    lv_obj_t *ui_lbl##NAME##_backup;      \
    lv_obj_t *ui_ico##NAME##_backup;      \
    lv_obj_t *ui_dro##NAME##_backup;

BACKUP_ELEMENTS
#undef BACKUP

void init_muxbackup(lv_obj_t *ui_pnlContent) {
#define BACKUP(NAME, UDATA) CREATE_OPTION_ITEM(backup, NAME);
    BACKUP_ELEMENTS
#undef BACKUP
}
