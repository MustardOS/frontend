#include "muxshare.h"
#include "muxhdmi.h"
#include "ui/ui_muxhdmi.h"
#include <string.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int resolution_original, space_original,
        depth_original, range_original, scan_original, audio_original;

#define UI_COUNT 6
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblResolution_hdmi, lang.MUXHDMI.HELP.RESOLUTION},
            {ui_lblSpace_hdmi,      lang.MUXHDMI.HELP.COLOUR.SPACE},
            {ui_lblDepth_hdmi,      lang.MUXHDMI.HELP.COLOUR.DEPTH},
            {ui_lblRange_hdmi,      lang.MUXHDMI.HELP.COLOUR.RANGE},
            {ui_lblScan_hdmi,       lang.MUXHDMI.HELP.SCAN_SCALE},
            {ui_lblAudio_hdmi,      lang.MUXHDMI.HELP.AUDIO_OUTPUT}
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void init_dropdown_settings() {
    resolution_original = lv_dropdown_get_selected(ui_droResolution_hdmi);
    space_original = lv_dropdown_get_selected(ui_droSpace_hdmi);
    depth_original = lv_dropdown_get_selected(ui_droDepth_hdmi);
    range_original = lv_dropdown_get_selected(ui_droRange_hdmi);
    scan_original = lv_dropdown_get_selected(ui_droScan_hdmi);
    audio_original = lv_dropdown_get_selected(ui_droAudio_hdmi);
}

static void restore_hdmi_options() {
    map_drop_down_to_index(ui_droResolution_hdmi, config.SETTINGS.HDMI.RESOLUTION,
                           (int[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 11, 0);
    lv_dropdown_set_selected(ui_droSpace_hdmi, config.SETTINGS.HDMI.SPACE);
    lv_dropdown_set_selected(ui_droDepth_hdmi, config.SETTINGS.HDMI.DEPTH);
    lv_dropdown_set_selected(ui_droRange_hdmi, config.SETTINGS.HDMI.RANGE);
    lv_dropdown_set_selected(ui_droScan_hdmi, config.SETTINGS.HDMI.SCAN);
    lv_dropdown_set_selected(ui_droAudio_hdmi, config.SETTINGS.HDMI.AUDIO);
}

static void save_hdmi_options() {
    int idx_resolution = map_drop_down_to_value(lv_dropdown_get_selected(ui_droResolution_hdmi),
                                                (int[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 12, 0);

    int idx_space = lv_dropdown_get_selected(ui_droSpace_hdmi);
    int idx_depth = lv_dropdown_get_selected(ui_droDepth_hdmi);
    int idx_range = lv_dropdown_get_selected(ui_droRange_hdmi);
    int idx_scan = lv_dropdown_get_selected(ui_droScan_hdmi);
    int idx_audio = lv_dropdown_get_selected(ui_droAudio_hdmi);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droResolution_hdmi) != resolution_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/resolution"), "w", INT, idx_resolution);
    }

    if (lv_dropdown_get_selected(ui_droSpace_hdmi) != space_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/space"), "w", INT, idx_space);
    }

    if (lv_dropdown_get_selected(ui_droDepth_hdmi) != depth_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/depth"), "w", INT, idx_depth);
    }

    if (lv_dropdown_get_selected(ui_droRange_hdmi) != range_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/range"), "w", INT, idx_range);
    }

    if (lv_dropdown_get_selected(ui_droScan_hdmi) != scan_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/scan"), "w", INT, idx_scan);
    }

    if (lv_dropdown_get_selected(ui_droAudio_hdmi) != audio_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/hdmi/audio"), "w", INT, idx_audio);
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0, 0);
        refresh_screen(ui_screen);
    }

    refresh_config = 1;
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlResolution_hdmi,
            ui_pnlSpace_hdmi,
            ui_pnlDepth_hdmi,
            ui_pnlRange_hdmi,
            ui_pnlScan_hdmi,
            ui_pnlAudio_hdmi
    };

    ui_objects[0] = ui_lblResolution_hdmi;
    ui_objects[1] = ui_lblSpace_hdmi;
    ui_objects[2] = ui_lblDepth_hdmi;
    ui_objects[3] = ui_lblRange_hdmi;
    ui_objects[4] = ui_lblScan_hdmi;
    ui_objects[5] = ui_lblAudio_hdmi;

    lv_obj_t *ui_objects_value[] = {
            ui_droResolution_hdmi,
            ui_droSpace_hdmi,
            ui_droDepth_hdmi,
            ui_droRange_hdmi,
            ui_droScan_hdmi,
            ui_droAudio_hdmi
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoResolution_hdmi,
            ui_icoSpace_hdmi,
            ui_icoDepth_hdmi,
            ui_icoRange_hdmi,
            ui_icoScan_hdmi,
            ui_icoAudio_hdmi
    };

    apply_theme_list_panel(ui_pnlResolution_hdmi);
    apply_theme_list_panel(ui_pnlSpace_hdmi);
    apply_theme_list_panel(ui_pnlDepth_hdmi);
    apply_theme_list_panel(ui_pnlRange_hdmi);
    apply_theme_list_panel(ui_pnlScan_hdmi);
    apply_theme_list_panel(ui_pnlAudio_hdmi);

    apply_theme_list_item(&theme, ui_lblResolution_hdmi, lang.MUXHDMI.RESOLUTION);
    apply_theme_list_item(&theme, ui_lblSpace_hdmi, lang.MUXHDMI.COLOUR.SPACE);
    apply_theme_list_item(&theme, ui_lblDepth_hdmi, lang.MUXHDMI.COLOUR.DEPTH);
    apply_theme_list_item(&theme, ui_lblRange_hdmi, lang.MUXHDMI.COLOUR.RANGE.TITLE);
    apply_theme_list_item(&theme, ui_lblScan_hdmi, lang.MUXHDMI.SCAN_SCALE.TITLE);
    apply_theme_list_item(&theme, ui_lblAudio_hdmi, lang.MUXHDMI.AUDIO_OUTPUT.TITLE);

    apply_theme_list_glyph(&theme, ui_icoResolution_hdmi, mux_module, "resolution");
    apply_theme_list_glyph(&theme, ui_icoSpace_hdmi, mux_module, "space");
    apply_theme_list_glyph(&theme, ui_icoDepth_hdmi, mux_module, "depth");
    apply_theme_list_glyph(&theme, ui_icoRange_hdmi, mux_module, "range");
    apply_theme_list_glyph(&theme, ui_icoScan_hdmi, mux_module, "scan");
    apply_theme_list_glyph(&theme, ui_icoAudio_hdmi, mux_module, "audio");

    apply_theme_list_drop_down(&theme, ui_droResolution_hdmi, NULL);
    apply_theme_list_drop_down(&theme, ui_droSpace_hdmi, NULL);

    char *depth_string = generate_number_string(8, 16, 2, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droDepth_hdmi, depth_string);
    free(depth_string);

    apply_theme_list_drop_down(&theme, ui_droRange_hdmi, NULL);
    apply_theme_list_drop_down(&theme, ui_droScan_hdmi, NULL);
    apply_theme_list_drop_down(&theme, ui_droAudio_hdmi, NULL);

    add_drop_down_options(ui_droResolution_hdmi, (char *[]) {
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
            "1080p + 60hz"}, 11);

    add_drop_down_options(ui_droSpace_hdmi, (char *[]) {
            "RGB",
            "YUV444",
            "YUV422",
            "YUV420"}, 4);
    add_drop_down_options(ui_droRange_hdmi, (char *[]) {
            lang.MUXHDMI.COLOUR.RANGE.LIMITED, lang.MUXHDMI.COLOUR.RANGE.FULL}, 2);
    add_drop_down_options(ui_droScan_hdmi, (char *[]) {
            lang.MUXHDMI.SCAN_SCALE.OVER, lang.MUXHDMI.SCAN_SCALE.UNDER}, 2);
    add_drop_down_options(ui_droAudio_hdmi, (char *[]) {
            lang.MUXHDMI.AUDIO_OUTPUT.EXTERNAL, lang.MUXHDMI.AUDIO_OUTPUT.INTERNAL}, 2);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

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
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);

    save_hdmi_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "hdmi");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

    lv_obj_set_user_data(ui_lblResolution_hdmi, "resolution");
    lv_obj_set_user_data(ui_lblSpace_hdmi, "space");
    lv_obj_set_user_data(ui_lblDepth_hdmi, "depth");
    lv_obj_set_user_data(ui_lblRange_hdmi, "range");
    lv_obj_set_user_data(ui_lblScan_hdmi, "scan");
    lv_obj_set_user_data(ui_lblAudio_hdmi, "audio");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

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
