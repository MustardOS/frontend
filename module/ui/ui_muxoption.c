#include "ui_muxshare.h"
#include "ui_muxoption.h"

#define OPTION(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_option;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_option;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_option;                                                                                  \
    lv_obj_t *ui_val_##NAME##_option;

OPTION_ELEMENTS
#undef OPTION

void init_muxoption(lv_obj_t *ui_pnl_content) {
#define OPTION(NAME, UDATA) CREATE_VALUE_ITEM(option, NAME);
    OPTION_ELEMENTS
#undef OPTION
}
