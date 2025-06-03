#include "muxshare.h"
#include "ui/ui_muxhdmi.h"

#define UI_COUNT 6

#define HDMI(NAME, UDATA) static int NAME##_original;
HDMI_ELEMENTS
#undef HDMI

int hdmi_index[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblResolution_hdmi, lang.MUXHDMI.HELP.RESOLUTION},
            {ui_lblSpace_hdmi,      lang.MUXHDMI.HELP.COLOUR.SPACE},
            {ui_lblDepth_hdmi,      lang.MUXHDMI.HELP.COLOUR.DEPTH},
            {ui_lblRange_hdmi,      lang.MUXHDMI.HELP.COLOUR.RANGE},
            {ui_lblScan_hdmi,       lang.MUXHDMI.HELP.SCAN_SCALE},
            {ui_lblAudio_hdmi,      lang.MUXHDMI.HELP.AUDIO_OUTPUT}
    };

    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);
    gen_help(element_focused, help_messages, num_messages);
}

static void init_dropdown_settings() {
#define HDMI(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_hdmi);
    HDMI_ELEMENTS
#undef HDMI
}

static void restore_hdmi_options() {
    map_drop_down_to_index(ui_droResolution_hdmi, config.SETTINGS.HDMI.RESOLUTION, hdmi_index, 11, 0);
    lv_dropdown_set_selected(ui_droSpace_hdmi, config.SETTINGS.HDMI.SPACE);
    lv_dropdown_set_selected(ui_droDepth_hdmi, config.SETTINGS.HDMI.DEPTH);
    lv_dropdown_set_selected(ui_droRange_hdmi, config.SETTINGS.HDMI.RANGE);
    lv_dropdown_set_selected(ui_droScan_hdmi, config.SETTINGS.HDMI.SCAN);
    lv_dropdown_set_selected(ui_droAudio_hdmi, config.SETTINGS.HDMI.AUDIO);
}

static void save_hdmi_options() {
    int is_modified = 0;

    CHECK_AND_SAVE_MAP(hdmi, Resolution, "settings/hdmi/resolution", hdmi_index, 11, 0);
    CHECK_AND_SAVE_STD(hdmi, Space, "settings/hdmi/space", INT);
    CHECK_AND_SAVE_STD(hdmi, Depth, "settings/hdmi/depth", INT);
    CHECK_AND_SAVE_STD(hdmi, Range, "settings/hdmi/range", INT);
    CHECK_AND_SAVE_STD(hdmi, Scan, "settings/hdmi/scan", INT);
    CHECK_AND_SAVE_STD(hdmi, Audio, "settings/hdmi/audio", INT);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);
        refresh_config = 1;
    }
}

static void init_navigation_group() {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *hdmi_resolution[] = {
            "480i",
            "576i",
            "480p",
            "576p",
            "720p + 50hz",
            "720p + 60hz",
            "1080i + 50hz",
            "1080i + 60hz",
            "1080p + 24hz",
            "1080p + 50hz",
            "1080p + 60hz"
    };

    char *hdmi_space[] = {
            "RGB",
            "YUV444",
            "YUV422",
            "YUV420"
    };

    char *hdmi_range[] = {
            lang.MUXHDMI.COLOUR.RANGE.LIMITED,
            lang.MUXHDMI.COLOUR.RANGE.FULL
    };

    char *hdmi_scan[] = {
            lang.MUXHDMI.SCAN_SCALE.OVER,
            lang.MUXHDMI.SCAN_SCALE.UNDER
    };

    char *hdmi_audio[] = {
            lang.MUXHDMI.AUDIO_OUTPUT.EXTERNAL,
            lang.MUXHDMI.AUDIO_OUTPUT.INTERNAL
    };

    INIT_OPTION_ITEM(-1, hdmi, Resolution, lang.MUXHDMI.RESOLUTION, "resolution", hdmi_resolution, 11);
    INIT_OPTION_ITEM(-1, hdmi, Space, lang.MUXHDMI.COLOUR.SPACE, "space", hdmi_space, 4);
    INIT_OPTION_ITEM(-1, hdmi, Depth, lang.MUXHDMI.COLOUR.DEPTH, "depth", NULL, 0);
    INIT_OPTION_ITEM(-1, hdmi, Range, lang.MUXHDMI.COLOUR.RANGE.TITLE, "range", hdmi_range, 2);
    INIT_OPTION_ITEM(-1, hdmi, Scan, lang.MUXHDMI.SCAN_SCALE.TITLE, "scan", hdmi_scan, 2);
    INIT_OPTION_ITEM(-1, hdmi, Audio, lang.MUXHDMI.AUDIO_OUTPUT.TITLE, "audio", hdmi_audio, 2);

    char *depth_string = generate_number_string(8, 16, 2, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droDepth_hdmi, depth_string);
    free(depth_string);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    save_hdmi_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "hdmi");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_standard_panels[0] = ui_pnlFooter;
    ui_mux_standard_panels[1] = ui_pnlHeader;
    ui_mux_standard_panels[2] = ui_pnlHelp;
    ui_mux_standard_panels[3] = ui_pnlProgressBrightness;
    ui_mux_standard_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_standard_panels, sizeof(ui_mux_standard_panels) / sizeof(ui_mux_standard_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

#define HDMI(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_hdmi, UDATA);
    HDMI_ELEMENTS
#undef HDMI

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

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_standard_panels, sizeof(ui_mux_standard_panels) / sizeof(ui_mux_standard_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxhdmi_main() {
    init_module("muxhdmi");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXHDMI.TITLE);
    init_muxhdmi(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_hdmi_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
