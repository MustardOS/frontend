#include "ui_muxshare.h"
#include "ui_muxkiosk.h"

#define KIOSK(NAME, UDATA)                                                                                             \
    lv_obj_t *ui_pnl_##NAME##_kiosk;                                                                                   \
    lv_obj_t *ui_lbl_##NAME##_kiosk;                                                                                   \
    lv_obj_t *ui_ico_##NAME##_kiosk;                                                                                   \
    lv_obj_t *ui_dro_##NAME##_kiosk;

KIOSK_ELEMENTS
#undef KIOSK

void init_muxkiosk(lv_obj_t *ui_pnl_content) {
#define KIOSK(NAME, UDATA) CREATE_OPTION_ITEM(kiosk, NAME);
    KIOSK_ELEMENTS
#undef KIOSK
}
