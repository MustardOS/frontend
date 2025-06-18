#include "muxshare.h"
#include "ui/ui_muxtext.h"

#define TEXT_FILE "/tmp/mux_text_read"

static void scroll_textarea(int step) {
    lv_coord_t y = lv_obj_get_scroll_y(ui_txtDocument_text);
    lv_coord_t max = lv_obj_get_height(lv_textarea_get_label(ui_txtDocument_text))
                     - lv_obj_get_height(ui_txtDocument_text);

    if (max < 0) max = 0;

    // Clamp to top if we overshoot
    if (step > 0 && y - step < 0) {
        lv_obj_scroll_to_y(ui_txtDocument_text, 0, LV_ANIM_ON);
        return;
    }

    // Clamp to bottom... hehe
    if (step < 0 && y - step > max) {
        lv_obj_scroll_to_y(ui_txtDocument_text, max, LV_ANIM_ON);
        return;
    }

    // Otherwise scroll normally
    if ((step > 0 && y > (step - 5)) ||
        (step < 0 && y < max)) {
        lv_obj_scroll_by(ui_txtDocument_text, 0, step, LV_ANIM_ON);
    }
}

static void handle_up(void) {
    scroll_textarea(+config.SETTINGS.ADVANCED.ACCELERATE);
}

static void handle_down(void) {
    scroll_textarea(-config.SETTINGS.ADVANCED.ACCELERATE);
}

static void handle_up_page(void) {
    scroll_textarea(+config.SETTINGS.ADVANCED.ACCELERATE * 3);
}

static void handle_down_page(void) {
    scroll_textarea(-config.SETTINGS.ADVANCED.ACCELERATE * 3);
}

static void handle_x() {
    lv_obj_scroll_to_y(ui_txtDocument_text, 0, LV_ANIM_ON);
}

static void handle_b() {
    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "text");

    close_input();
    mux_input_stop();
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {ui_lblNavXGlyph, "",                0},
            {ui_lblNavX,      lang.GENERIC.TOP,  0},
            {NULL, NULL,                         0}
    });

    lv_obj_set_user_data(ui_txtDocument_text, "document");

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxtext_main() {
    if (!file_exist(TEXT_FILE)) return 1;

    init_module("muxtext");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, read_line_char_from(TEXT_FILE, 1));
    init_muxtext(ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_timer(ui_refresh_task, NULL);

    lv_textarea_set_text(ui_txtDocument_text, read_all_char_from(read_line_char_from(TEXT_FILE, 2)));
    handle_x();

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_L1] = handle_up_page,
                    [MUX_INPUT_R1] = handle_down_page,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_L1] = handle_up_page,
                    [MUX_INPUT_R1] = handle_down_page,
            },

    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
