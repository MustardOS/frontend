#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define BTALL(NAME, UDATA)                                                                                             \
    lv_obj_t *ui_pnl_##NAME##_btall;                                                                                   \
    lv_obj_t *ui_lbl_##NAME##_btall;                                                                                   \
    lv_obj_t *ui_ico_##NAME##_btall;                                                                                   \
    lv_obj_t *ui_dro_##NAME##_btall;

BTALL_ELEMENTS
#undef BTALL

void init_muxbtall(lv_obj_t *ui_pnl_content) {
#define BTALL(NAME, UDATA) CREATE_OPTION_ITEM(btall, NAME);
    BTALL_ELEMENTS
#undef BTALL
}
