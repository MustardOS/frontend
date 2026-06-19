#include "muxshare.h"

static void cancel_scan(void);

static int scan_pending = 0;
static time_t scan_start = 0;
static lv_timer_t *scan_poll_timer = NULL;

static void show_help(void) {
    show_info_box(lang.MUXNETSCAN.TITLE, lang.MUXNETSCAN.HELP, 0);
}

static void populate_network_items(void) {
    reset_ui_groups();

    char *scan_file = "/tmp/net_scan";
    FILE *file = fopen(scan_file, "r");
    if (!file) return;

    if (strcmp(read_line_char_from(scan_file, 1), "[!]") == 0) {
        fclose(file);
        return;
    }

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
}

static void net_scan_poll_task(lv_timer_t *t) {
    if (!scan_pending) {
        lv_timer_del(t);
        scan_poll_timer = NULL;
        return;
    }

    struct stat st;
    int file_ready = (stat("/tmp/net_scan", &st) == 0);
    int timed_out = (time(NULL) - scan_start >= 30);

    if (!file_ready && !timed_out) return;

    scan_pending = 0;
    lv_timer_del(t);
    scan_poll_timer = NULL;

    populate_network_items();

    lv_label_set_text(ui_lblScreenMessage, !ui_count ? lang.MUXNETSCAN.NONE : "");

    if (ui_count > 0) gen_step_movement(0, +1, 1, 0, 0);
}

static void create_network_items(void) {
    lv_label_set_text(ui_lblScreenMessage, lang.MUXNETSCAN.SCAN);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    remove("/tmp/net_scan");

    scan_start = time(NULL);
    scan_pending = 1;

    const char *args[] = {(OPT_PATH "script/web/ssid.sh"), NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static void cancel_scan(void) {
    if (scan_poll_timer) {
        lv_timer_del(scan_poll_timer);
        scan_poll_timer = NULL;
    }

    scan_pending = 0;
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);
    cancel_scan();

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid", CHAR, lv_label_get_text(lv_group_get_focused(ui_group)));
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/pass", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/address", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/subnet", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/gateway", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/dns", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, "");

    refresh_config = 1;
    load_mux("network");

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(SND_BACK);
    cancel_scan();

    mux_input_stop();
}

static void handle_rescan(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);
    cancel_scan();
    load_mux("net_scan");

    mux_input_stop();
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

    init_timer(ui_gen_refresh_task, NULL);
    scan_poll_timer = lv_timer_create(net_scan_poll_task, 500, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_rescan,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
