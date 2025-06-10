#include "muxshare.h"
#include "../lvgl/src/drivers/display/sdl.h"

int main(void) {
    init_module("muxwarn");

    load_device(&device);
    load_config(&config);

    init_theme(0, 0);
    init_display(0);

    const lv_font_t *header_font;
    if (strcasecmp(device.DEVICE.NAME, "tui-brick") == 0) {
        header_font = &ui_font_NotoSansBigHD;
    } else {
        header_font = &ui_font_NotoSansBig;
    }

    lv_obj_t *ui_scrWarn = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrWarn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scrWarn, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrWarn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_conWarn = lv_obj_create(ui_scrWarn);
    lv_obj_remove_style_all(ui_conWarn);
    lv_obj_set_width(ui_conWarn, lv_pct(100));
    lv_obj_set_height(ui_conWarn, lv_pct(100));
    lv_obj_set_style_bg_color(ui_conWarn, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conWarn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(ui_conWarn, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conWarn, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conWarn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ui_lblHeader = lv_label_create(ui_conWarn);
    lv_obj_set_width(ui_lblHeader, lv_pct(90));
    lv_obj_set_height(ui_lblHeader, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHeader, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblHeader, "muOS Disclaimer\n");
    lv_obj_set_style_text_color(ui_lblHeader, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeader, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeader, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_font(ui_lblHeader, header_font, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblMessage = lv_label_create(ui_conWarn);
    lv_obj_set_width(ui_lblMessage, lv_pct(90));
    lv_obj_set_height(ui_lblMessage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblMessage, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblMessage, "MustardOS (muOS) is built with respect for developers, artists, and content "
                                     "creators. While we understand how common content sharing is, muOS does not "
                                     "include or support pirated content. The system and tools are designed to work "
                                     "with legally obtained files, and it is up to users to source their own content "
                                     "responsibly.\n\nBy installing and using muOS, you acknowledge this approach and "
                                     "agree to respect the work of those who make the content we all enjoy.");
    lv_obj_set_style_text_color(ui_lblMessage, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMessage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblMessage, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_t *ui_barProgress = lv_bar_create(ui_scrWarn);
    lv_bar_set_range(ui_barProgress, 0, 100);
    lv_obj_set_y(ui_barProgress, 0);
    lv_obj_set_size(ui_barProgress, device.MUX.WIDTH, 24);
    lv_obj_set_style_pad_all(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgress, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgress, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_disp_load_scr(ui_scrWarn);
    load_font_text(ui_scrWarn);

    int progress = 0;

    while (1) {
        refresh_screen(ui_scrWarn);
        sleep(1);

        if (progress > 100) break;

        progress += 5;
        lv_bar_set_value(ui_barProgress, progress, LV_ANIM_ON);
    }

    sdl_cleanup();
    return 0;
}
