#include "ui_muxshare.h"
#include "ui_muxstorage.h"

#define STORAGE(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_storage;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_storage;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_storage;                                                                                 \
    lv_obj_t *ui_val_##NAME##_storage;

STORAGE_ELEMENTS
#undef STORAGE

void init_muxstorage(lv_obj_t *ui_pnl_content) {
#define STORAGE(NAME, UDATA) CREATE_VALUE_ITEM(storage, NAME);
    STORAGE_ELEMENTS
#undef STORAGE
}
