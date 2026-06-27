#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define CONNECT(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_connect;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_connect;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_connect;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_connect;

CONNECT_ELEMENTS
#undef CONNECT

void init_muxconnect(lv_obj_t *ui_pnl_content) {
#define CONNECT(NAME, UDATA) CREATE_OPTION_ITEM(connect, NAME);
    CONNECT_ELEMENTS
#undef CONNECT
}
