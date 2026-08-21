#include "ui_muxshare.h"
#include "ui_muxlink.h"

#define LINK(NAME, UDATA)                                                                                              \
    lv_obj_t *ui_pnl_##NAME##_link;                                                                                    \
    lv_obj_t *ui_lbl_##NAME##_link;                                                                                    \
    lv_obj_t *ui_ico_##NAME##_link;                                                                                    \
    lv_obj_t *ui_val_##NAME##_link;

LINK_ELEMENTS
#undef LINK

void init_muxlink(lv_obj_t *ui_pnl_content) {
#define LINK(NAME, UDATA) CREATE_VALUE_ITEM(link, NAME);
    LINK_ELEMENTS
#undef LINK
}
