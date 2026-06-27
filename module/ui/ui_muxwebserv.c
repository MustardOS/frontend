#include "ui_muxshare.h"
#include "ui_muxwebserv.h"

#define WEBSERV(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_webserv;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_webserv;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_webserv;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_webserv;

WEBSERV_ELEMENTS
#undef WEBSERV

void init_muxwebserv(lv_obj_t *ui_pnl_content) {
#define WEBSERV(NAME, UDATA) CREATE_OPTION_ITEM(webserv, NAME);
    WEBSERV_ELEMENTS
#undef WEBSERV
}
