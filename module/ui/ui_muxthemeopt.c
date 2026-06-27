#include "ui_muxshare.h"
#include "ui_muxthemeopt.h"

#define THEMEOPT(NAME, UDATA)                                                                                          \
    lv_obj_t *ui_pnl_##NAME##_themeopt;                                                                                \
    lv_obj_t *ui_lbl_##NAME##_themeopt;                                                                                \
    lv_obj_t *ui_ico_##NAME##_themeopt;                                                                                \
    lv_obj_t *ui_dro_##NAME##_themeopt;

THEMEOPT_ELEMENTS
#undef THEMEOPT

void init_muxthemeopt(lv_obj_t *ui_pnl_content) {
#define THEMEOPT(NAME, UDATA) CREATE_OPTION_ITEM(themeopt, NAME);
    THEMEOPT_ELEMENTS
#undef THEMEOPT
}
