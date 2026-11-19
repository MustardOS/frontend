#include "ui_muxshare.h"
#include "ui_muxadjust.h"

#define ADJUST(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_adjust;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_adjust;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_adjust;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_adjust;

ADJUST_ELEMENTS
#undef ADJUST

void init_muxadjust(lv_obj_t *ui_pnl_content) {
#define ADJUST(NAME, UDATA) CREATE_OPTION_ITEM(adjust, NAME);
    ADJUST_ELEMENTS
#undef ADJUST
}
