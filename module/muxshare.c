#include "muxshare.h"

size_t item_count = 0;
content_item *items = NULL;

int refresh_kiosk = 0;
int refresh_config = 0;
int refresh_resolution = 0;

int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;
lv_group_t *ui_group_value;

void setup_nav(struct nav_bar *nav_items) {
    for (size_t i = 0; nav_items[i].item != NULL; i++) {
        if (nav_items[i].ui_check && !ui_count) continue;

        if (nav_items[i].text && nav_items[i].text[0] != '\0') {
            lv_label_set_text(nav_items[i].item, nav_items[i].text);
        }

        lv_obj_clear_flag(nav_items[i].item, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_move_foreground(nav_items[i].item);
    }
}

void header_and_footer_setup() {
    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");
}

void overlay_display() {
#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    if (kiosk.ENABLE) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}
