#include "ui_muxshare.h"
#include "ui_muxappcon.h"

#define APPCON(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_appcon; \
    lv_obj_t *ui_lbl##NAME##_appcon; \
    lv_obj_t *ui_ico##NAME##_appcon; \
    lv_obj_t *ui_lbl##NAME##Value_appcon;

APPCON_ELEMENTS
#undef APPCON

void init_muxappcon(lv_obj_t *ui_pnlContent) {
#define APPCON(NAME, UDATA) CREATE_VALUE_ITEM(appcon, NAME);
    APPCON_ELEMENTS
#undef APPCON

}
