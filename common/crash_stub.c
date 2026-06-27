#include "crash.h"

void crash_init(const char *module_name) {
    (void) module_name;
}

void crash_ui_check(
    const struct theme_config *t, const struct mux_lang *l, const lv_obj_t *layer, const int *msgbox_active
) {
    (void) t;
    (void) l;
    (void) layer;
    (void) msgbox_active;
}

void crash_ui_apply_font(const lv_obj_t *source) {
    (void) source;
}

int crash_ui_dismiss(void) {
    return 0;
}

void power_loss_ui_apply_font(const lv_obj_t *source) {
    (void) source;
}

void power_loss_ui_check(
    const struct theme_config *t, const struct mux_lang *l, const lv_obj_t *layer, const int *msgbox_active
) {
    (void) t;
    (void) l;
    (void) layer;
    (void) msgbox_active;
}

int power_loss_ui_dismiss(void) {
    return 0;
}
