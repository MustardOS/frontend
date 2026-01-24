#include "ui_muxshare.h"
#include "ui_muxchrony.h"

#define CHRONY(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_chrony;      \
    lv_obj_t *ui_lbl##NAME##_chrony;      \
    lv_obj_t *ui_ico##NAME##_chrony;      \
    lv_obj_t *ui_lbl##NAME##Value_chrony;

CHRONY_ELEMENTS
#undef CHRONY

void init_muxchrony(lv_obj_t *ui_pnlContent) {
#define CHRONY(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(chrony, NAME);
    CHRONY_ELEMENTS
#undef CHRONY
}
