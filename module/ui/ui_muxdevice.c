#include "ui_muxshare.h"
#include "ui_muxdanger.h"

#define DEVICE(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_device; \
    lv_obj_t *ui_lbl##NAME##_device; \
    lv_obj_t *ui_ico##NAME##_device; \
    lv_obj_t *ui_dro##NAME##_device;

DEVICE_ELEMENTS
#undef DEVICE

void init_muxdevice(lv_obj_t *ui_pnlContent) {
#define DEVICE(NAME, UDATA) CREATE_OPTION_ITEM(device, NAME);
    DEVICE_ELEMENTS
#undef DEVICE
}
