#include "ui_muxshare.h"
#include "ui_muxstorage.h"

#define STORAGE(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_storage;      \
    lv_obj_t *ui_lbl##NAME##_storage;      \
    lv_obj_t *ui_ico##NAME##_storage;      \
    lv_obj_t *ui_lbl##NAME##Value_storage;

STORAGE_ELEMENTS
#undef STORAGE

void init_muxstorage(lv_obj_t *ui_pnlContent) {
#define STORAGE(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(storage, NAME);
    STORAGE_ELEMENTS
#undef STORAGE
}
