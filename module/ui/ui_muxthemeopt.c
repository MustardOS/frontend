#include "ui_muxshare.h"
#include "ui_muxthemeopt.h"

#define THEMEOPT(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_themeopt; \
    lv_obj_t *ui_lbl##NAME##_themeopt; \
    lv_obj_t *ui_ico##NAME##_themeopt; \
    lv_obj_t *ui_dro##NAME##_themeopt;

THEMEOPT_ELEMENTS
#undef THEMEOPT

void init_muxthemeopt(lv_obj_t *ui_pnlContent) {
#define THEMEOPT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(themeopt, NAME);
    THEMEOPT_ELEMENTS
#undef THEMEOPT
}
