#include "ui_muxshare.h"
#include "ui_muxdanger.h"

#define DEVICE(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_device;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_device;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_device;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_device;

DEVICE_ELEMENTS
#undef DEVICE

void init_muxdevice(lv_obj_t *ui_pnl_content) {
#define DEVICE(NAME, UDATA) CREATE_OPTION_ITEM(device, NAME);
    DEVICE_ELEMENTS
#undef DEVICE
}
