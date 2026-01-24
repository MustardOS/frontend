#include "ui_muxshare.h"
#include "ui_muxinstall.h"

#define INSTALL(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_install; \
    lv_obj_t *ui_lbl##NAME##_install; \
    lv_obj_t *ui_ico##NAME##_install;

INSTALL_ELEMENTS
#undef INSTALL

void init_muxinstall(lv_obj_t *ui_pnlContent) {
#define INSTALL(NAME, ENUM, UDATA) CREATE_STATIC_ITEM(install, NAME);
    INSTALL_ELEMENTS
#undef INSTALL
}
