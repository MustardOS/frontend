#include "ui_muxshare.h"
#include "ui_muxcontent.h"

#define CONTENT(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_content;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_content;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_content;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_content;

CONTENT_ELEMENTS
#undef CONTENT

void init_muxcontent(lv_obj_t *ui_pnl_content) {
#define CONTENT(NAME, UDATA) CREATE_OPTION_ITEM(content, NAME);
    CONTENT_ELEMENTS
#undef CONTENT
}
