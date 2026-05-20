#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define BTALL(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_btall; \
    lv_obj_t *ui_lbl##NAME##_btall; \
    lv_obj_t *ui_ico##NAME##_btall; \
    lv_obj_t *ui_dro##NAME##_btall;

BTALL_ELEMENTS
#undef BTALL

void init_muxbtall(lv_obj_t *ui_pnlContent) {
#define BTALL(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(btall, NAME);
    BTALL_ELEMENTS
#undef BTALL
}
