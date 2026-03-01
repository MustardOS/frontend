#include "ui_muxshare.h"
#include "ui_muxtweakadv.h"

#define TWEAKADV(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_tweakadv; \
    lv_obj_t *ui_lbl##NAME##_tweakadv; \
    lv_obj_t *ui_ico##NAME##_tweakadv; \
    lv_obj_t *ui_dro##NAME##_tweakadv;

TWEAKADV_ELEMENTS
#undef TWEAKADV

void init_muxtweakadv(lv_obj_t *ui_pnlContent) {
#define TWEAKADV(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(tweakadv, NAME);
    TWEAKADV_ELEMENTS
#undef TWEAKADV
}

const int accelerate_values[] = {
        32767, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256
};

const int repeat_delay_values[] = {
        32767, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256,
        272, 288, 304, 320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480, 496, 512
};

const int swap_values[] = {
        0, 128, 256, 384, 512, 640, 768, 896, 1024
};

const int battery_offset_values[] = { // ugh!
        -50, -49, -48, -47, -46, -45, -44, -43, -42, -41,
        -40, -39, -38, -37, -36, -35, -34, -33, -32, -31,
        -30, -29, -28, -27, -26, -25, -24, -23, -22, -21,
        -20, -19, -18, -17, -16, -15, -14, -13, -12, -11,
        -10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
        0,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50
};
