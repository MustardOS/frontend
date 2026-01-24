#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define CONNECT(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_connect; \
    lv_obj_t *ui_lbl##NAME##_connect; \
    lv_obj_t *ui_ico##NAME##_connect; \
    lv_obj_t *ui_dro##NAME##_connect;

CONNECT_ELEMENTS
#undef CONNECT

void init_muxconnect(lv_obj_t *ui_pnlContent) {
#define CONNECT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(connect, NAME);
    CONNECT_ELEMENTS
#undef CONNECT
}
