#include "ui_muxshare.h"
#include "ui_muxcustom.h"

#define CUSTOM(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_custom; \
    lv_obj_t *ui_lbl##NAME##_custom; \
    lv_obj_t *ui_ico##NAME##_custom; \
    lv_obj_t *ui_dro##NAME##_custom;

CUSTOM_ELEMENTS
#undef CUSTOM

void init_muxcustom(lv_obj_t *ui_pnlContent) {
#define CUSTOM(NAME, UDATA) CREATE_OPTION_ITEM(custom, NAME);
    CUSTOM_ELEMENTS
#undef CUSTOM
}
