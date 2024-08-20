#include "../lvgl/lvgl.h"
#include "ui/ui.h"
#include "../common/theme.h"

struct theme_config theme;

struct big {
    lv_obj_t *e;
    uint32_t c;
};

struct small {
    lv_obj_t *e;
    int16_t c;
};

void apply_theme() {
    // Absolutely nothing to do here!
}
