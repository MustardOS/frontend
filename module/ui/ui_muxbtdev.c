#include "ui_muxshare.h"
#include "ui_muxbtdev.h"

#define BTDEV_INFO(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_btdev;      \
    lv_obj_t *ui_lbl##NAME##_btdev;      \
    lv_obj_t *ui_ico##NAME##_btdev;      \
    lv_obj_t *ui_lbl##NAME##Value_btdev;

BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, ENUM, UDATA) \
    lv_obj_t *ui_pnl##NAME##_btdev;  \
    lv_obj_t *ui_lbl##NAME##_btdev;  \
    lv_obj_t *ui_ico##NAME##_btdev;  \
    lv_obj_t *ui_dro##NAME##_btdev;

BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT

void init_muxbtdev(lv_obj_t *ui_pnlContent) {
#define BTDEV_INFO(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(btdev, NAME);
    BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(btdev, NAME);
    BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT
}
