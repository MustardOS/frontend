#include "ui_muxshare.h"
#include "ui_muxaccess.h"

#define ACCESS(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_access;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_access;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_access;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_access;

ACCESS_ELEMENTS
#undef ACCESS

void init_muxaccess(lv_obj_t *ui_pnl_content) {
#define ACCESS(NAME, UDATA) CREATE_OPTION_ITEM(access, NAME);
    ACCESS_ELEMENTS
#undef ACCESS
}
