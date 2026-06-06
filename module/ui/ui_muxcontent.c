#include "ui_muxshare.h"
#include "ui_muxcontent.h"

#define CONTENT(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_content; \
    lv_obj_t *ui_lbl##NAME##_content; \
    lv_obj_t *ui_ico##NAME##_content; \
    lv_obj_t *ui_dro##NAME##_content;

CONTENT_ELEMENTS
#undef CONTENT

void init_muxcontent(lv_obj_t *ui_pnlContent) {
#define CONTENT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(content, NAME);
    CONTENT_ELEMENTS
#undef CONTENT
}
