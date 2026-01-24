#include "ui_muxshare.h"
#include "ui_muxwebserv.h"

#define WEBSERV(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_webserv; \
    lv_obj_t *ui_lbl##NAME##_webserv; \
    lv_obj_t *ui_ico##NAME##_webserv; \
    lv_obj_t *ui_dro##NAME##_webserv;

WEBSERV_ELEMENTS
#undef WEBSERV

void init_muxwebserv(lv_obj_t *ui_pnlContent) {
#define WEBSERV(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(webserv, NAME);
    WEBSERV_ELEMENTS
#undef WEBSERV
}
