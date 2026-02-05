#include "ui_muxshare.h"
#include "ui_muxsort.h"

#define SORT(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_sort; \
    lv_obj_t *ui_lbl##NAME##_sort; \
    lv_obj_t *ui_ico##NAME##_sort; \
    lv_obj_t *ui_dro##NAME##_sort;

SORT_ELEMENTS
#undef SORT

void init_muxsort(lv_obj_t *ui_pnlContent) {
#define SORT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(sort, NAME);
    SORT_ELEMENTS
#undef SORT
}
