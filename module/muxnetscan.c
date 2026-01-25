#include "muxshare.h"

static void show_help(void) {
    show_info_box(lang.MUXNETSCAN.TITLE, lang.MUXNETSCAN.HELP, 0);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);
    write_text_to_file(CONF_CONFIG_PATH "network/ssid", "w", CHAR,
                       lv_label_get_text(lv_group_get_focused(ui_group)));
    write_text_to_file(CONF_CONFIG_PATH "network/pass", "w", CHAR, "");
    write_text_to_file(CONF_CONFIG_PATH "network/address", "w", CHAR, "");
    write_text_to_file(CONF_CONFIG_PATH "network/subnet", "w", CHAR, "");
    write_text_to_file(CONF_CONFIG_PATH "network/gateway", "w", CHAR, "");
    write_text_to_file(CONF_CONFIG_PATH "network/dns", "w", CHAR, "");

    refresh_config = 1;

    close_input();
    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_rescan(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);
    load_mux("net_scan");

    close_input();
    mux_input_stop();
}

static void create_network_items(void) {
    lv_label_set_text(ui_lblScreenMessage, lang.MUXNETSCAN.SCAN);

    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    const char *args[] = {(OPT_PATH "script/web/ssid.sh"), NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    reset_ui_groups();

    char *scan_file = "/tmp/net_scan";
    FILE *file = fopen(scan_file, "r");
    if (!file || strcmp(read_line_char_from(scan_file, 1), "[!]") == 0) return;

    char ssid[40];
    while (fgets(ssid, sizeof(ssid), file)) {
        str_remchar(ssid, '\n');
        if (strlen(ssid) == 0) continue;

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
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblNetScanItem);
    }
    fclose(file);

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    list_nav_next(0);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.USE,    1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.RESCAN, 0},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

int muxnetscan_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETSCAN.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();

    create_network_items();

    init_elements();

    lv_label_set_text(ui_lblScreenMessage, !ui_count ? lang.MUXNETSCAN.NONE : "");

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_rescan,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
