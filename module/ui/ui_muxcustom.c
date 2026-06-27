#include "ui_muxshare.h"
#include "ui_muxcustom.h"

#define CUSTOM(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_custom;

CUSTOM_ELEMENTS
#undef CUSTOM

void init_muxcustom(lv_obj_t *ui_pnl_content) {
#define CUSTOM(NAME, UDATA) CREATE_OPTION_ITEM(custom, NAME);
    CUSTOM_ELEMENTS
#undef CUSTOM
}
