#include "ui_muxshare.h"
#include "ui_muxtweakadv.h"

#define TWEAKADV(NAME)                 \
    lv_obj_t *ui_pnl##NAME##_tweakadv; \
    lv_obj_t *ui_lbl##NAME##_tweakadv; \
    lv_obj_t *ui_ico##NAME##_tweakadv; \
    lv_obj_t *ui_dro##NAME##_tweakadv;

TWEAKADV_ELEMENTS
#undef TWEAKADV

void init_muxtweakadv(lv_obj_t *ui_pnlContent) {
#define TWEAKADV(NAME) CREATE_ELEMENT_ITEM(tweakadv, NAME);
    TWEAKADV_ELEMENTS
#undef TWEAKADV
}

const char *volume_values[] = {
        "previous", "silent", "soft", "loud"
};

const char *brightness_values[] = {
        "previous", "low", "medium", "high"
};

const char *state_values[] = {
        "mem", "freeze"
};

const char *cardmode_values[] = {
        "deadline", "noop"
};

const int accelerate_values[] = {
        32767, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256
};

const int swap_values[] = {
        0, 64, 128, 192, 256, 320, 384, 448, 512
};

const int zram_values[] = {
        0, 64, 128, 192, 256, 320, 384, 448, 512
};
