#include "ui_muxshare.h"
#include "ui_muxappcon.h"

#define APPCON(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_appcon;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_appcon;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_appcon;                                                                                  \
    lv_obj_t *ui_val_##NAME##_appcon;

APPCON_ELEMENTS
#undef APPCON

void init_muxappcon(lv_obj_t *ui_pnl_content) {
#define APPCON(NAME, UDATA) CREATE_VALUE_ITEM(appcon, NAME);
    APPCON_ELEMENTS
#undef APPCON
}
