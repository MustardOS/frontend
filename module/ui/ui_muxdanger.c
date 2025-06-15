#include "ui_muxshare.h"
#include "ui_muxdanger.h"

#define DANGER(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_danger; \
    lv_obj_t *ui_lbl##NAME##_danger; \
    lv_obj_t *ui_ico##NAME##_danger; \
    lv_obj_t *ui_dro##NAME##_danger;

DANGER_ELEMENTS
#undef DANGER

void init_muxdanger(lv_obj_t *ui_pnlContent) {
#define DANGER(NAME, UDATA) CREATE_OPTION_ITEM(danger, NAME);
    DANGER_ELEMENTS
#undef DANGER
}

const int four_values[] = {
        0, 4, 8, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100
};

const int merge_values[] = {
        0, 1, 2
};

const int request_values[] = {
        64, 128, 196, 256, 320, 384, 448, 512
};

const int read_ahead_values[] = {
        64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384
};

const int time_slice_values[] = {
        10, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000
};
