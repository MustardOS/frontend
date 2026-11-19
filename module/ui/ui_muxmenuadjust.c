#include "ui_muxshare.h"
#include "ui_muxmenuadjust.h"

#define MENUADJUST(NAME, UDATA)                                                                                        \
    lv_obj_t *ui_pnl_##NAME##_menuadjust;                                                                              \
    lv_obj_t *ui_lbl_##NAME##_menuadjust;                                                                              \
    lv_obj_t *ui_ico_##NAME##_menuadjust;                                                                              \
    lv_obj_t *ui_dro_##NAME##_menuadjust;

MENUADJUST_ELEMENTS
#undef MENUADJUST

void init_muxmenuadjust(lv_obj_t *ui_pnl_content) {
#define MENUADJUST(NAME, UDATA) CREATE_OPTION_ITEM(menuadjust, NAME);
    MENUADJUST_ELEMENTS
#undef MENUADJUST
}
