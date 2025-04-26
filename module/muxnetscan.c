#include "muxshare.h"
#include "muxnetscan.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static lv_obj_t *ui_mux_panels[5];

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXNETSCAN.TITLE, lang.MUXNETSCAN.HELP);
}

static void scan_networks() {
    lv_label_set_text(ui_lblScreenMessage, lang.MUXNETSCAN.SCAN);

    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    run_exec((const char *[]) {(INTERNAL_PATH "script/web/ssid.sh"), NULL});
}

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void create_network_items() {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    char *scan_file = "/tmp/net_scan";
    FILE *file = fopen(scan_file, "r");
    if (!file || strcmp(read_line_from_file(scan_file, 1), "[!]") == 0) return;

    char ssid[40];
    while (fgets(ssid, sizeof(ssid), file)) {
        str_remchar(ssid, '\n');
        if (!strlen(ssid)) continue;

        ui_count++;

        lv_obj_t *ui_pnlNetScan = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlNetScan);
        lv_obj_set_user_data(ui_pnlNetScan, strdup(str_nonew(ssid)));

        lv_obj_t *ui_lblNetScanItem = lv_label_create(ui_pnlNetScan);
        apply_theme_list_item(&theme, ui_lblNetScanItem, str_nonew(ssid));

        lv_obj_t *ui_lblNetScanGlyph = lv_img_create(ui_pnlNetScan);
        apply_theme_list_glyph(&theme, ui_lblNetScanGlyph, mux_module, "netscan");

        lv_group_add_obj(ui_group, ui_lblNetScanItem);
        lv_group_add_obj(ui_group_glyph, ui_lblNetScanGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlNetScan);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblNetScanItem, ui_lblNetScanGlyph, str_nonew(ssid));
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblNetScanItem, str_nonew(ssid));
    }
    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    list_nav_next(0);
    fclose(file);
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    play_sound("confirm", nav_sound, 0, 1);
    write_text_to_file((RUN_GLOBAL_PATH "network/ssid"), "w", CHAR,
                       lv_label_get_text(lv_group_get_focused(ui_group)));
    write_text_to_file((RUN_GLOBAL_PATH "network/pass"), "w", CHAR, "");
    write_text_to_file((RUN_GLOBAL_PATH "network/address"), "w", CHAR, "");
    write_text_to_file((RUN_GLOBAL_PATH "network/subnet"), "w", CHAR, "");
    write_text_to_file((RUN_GLOBAL_PATH "network/gateway"), "w", CHAR, "");
    write_text_to_file((RUN_GLOBAL_PATH "network/dns"), "w", CHAR, "");

    refresh_config = 1;
    
    close_input();
    mux_input_stop();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    close_input();
    mux_input_stop();
}

static void handle_rescan(void) {
    if (msgbox_active) return;

    play_sound("confirm", nav_sound, 0, 1);
    load_mux("net_scan");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help();
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

    lv_label_set_text(ui_lblNavA, lang.GENERIC.USE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.RESCAN);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB,
            ui_lblNavXGlyph,
            ui_lblNavX
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxnetscan_main() {

    init_module("muxnetscan");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETSCAN.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_sound(&nav_sound, mux_module);

    scan_networks();
    create_network_items();

    lv_label_set_text(ui_lblScreenMessage, !ui_count ? lang.MUXNETSCAN.NONE : "");
    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_rescan,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
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
