#include "muxshare.h"

static void cancel_scan(void);

static int scan_pending = 0;
static time_t scan_start = 0;
static lv_timer_t *scan_poll_timer = NULL;

static void show_help(void) {
    show_info_box(lang.MUXBTCON.TITLE, lang.MUXBTCON.HELP, 0);
}

static void populate_bt_scan_items(void) {
    reset_ui_groups();

    char *scan_file = CONF_CONFIG_PATH "bluetooth/scan";
    FILE *file = fopen(scan_file, "r");
    if (!file) return;

    char line[96];
    while (fgets(line, sizeof(line), file)) {
        str_remchar(line, '\n');
        if (strlen(line) == 0) continue;

        char mac[18] = {0};
        char name[64] = {0};
        if (sscanf(line, "%17s %63[^\n]", mac, name) < 2) continue;

        ui_count++;

        lv_obj_t *ui_pnlBtScan = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlBtScan);
        lv_obj_set_user_data(ui_pnlBtScan, strdup(mac));

        lv_obj_t *ui_lblBtScanItem = lv_label_create(ui_pnlBtScan);
        apply_theme_list_item(&theme, ui_lblBtScanItem, name);

        lv_obj_t *ui_lblBtScanGlyph = lv_img_create(ui_pnlBtScan);
        apply_theme_list_glyph(&theme, ui_lblBtScanGlyph, mux_module, "bluetooth");

        lv_group_add_obj(ui_group, ui_lblBtScanItem);
        lv_group_add_obj(ui_group_glyph, ui_lblBtScanGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlBtScan);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblBtScanItem, ui_lblBtScanGlyph, name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblBtScanItem);
    }

    fclose(file);

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

static void bt_scan_poll_task(lv_timer_t *t) {
    if (!scan_pending) {
        lv_timer_del(t);
        scan_poll_timer = NULL;
        return;
    }

    struct stat st;
    int file_ready = (stat(CONF_CONFIG_PATH "bluetooth/scan", &st) == 0);
    int timed_out = (time(NULL) - scan_start >= 30);

    if (!file_ready && !timed_out) return;

    scan_pending = 0;
    lv_timer_del(t);
    scan_poll_timer = NULL;

    populate_bt_scan_items();

    lv_label_set_text(ui_lblScreenMessage, !ui_count ? lang.MUXBTCON.NONE : "");

    if (ui_count > 0) {
        nav_silent = 1;
        gen_step_movement(0, +1, 1, 0);
        nav_silent = 0;
    }
}

static void create_bt_scan_items(void) {
    lv_label_set_text(ui_lblScreenMessage, lang.MUXBTCON.SCAN);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    remove(CONF_CONFIG_PATH "bluetooth/scan");

    scan_start = time(NULL);
    scan_pending = 1;

    const char *args[] = {(OPT_PATH "script/mux/bt_scan.sh"), "list", NULL};
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

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *mac = (const char *) lv_obj_get_user_data(panel);
    if (!mac) return;

    play_sound(SND_CONFIRM);
    cancel_scan();

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    toast_message(lang.MUXBTCON.CONNECT, FOREVER);

    const char *args[] = {(OPT_PATH "script/mux/bt_scan.sh"), "connect", mac_copy, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    load_mux("btall");
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
    load_mux("btcon");

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || !ui_count) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *mac = (const char *) lv_obj_get_user_data(panel);
    if (!mac) return;

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    toast_message(lang.GENERIC.REFRESH_RUN, FOREVER);

    const char *args[] = {(OPT_PATH "script/mux/bt_scan.sh"), "info", mac_copy, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    lv_obj_set_style_opa(ui_pnlMessage, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    char info_buf[512] = {0};
    FILE *f = fopen("/run/muos/bt_info", "r");
    if (f) {
        __attribute__((unused)) size_t _r = fread(info_buf, 1, sizeof(info_buf) - 1, f);
        fclose(f);
    }

    lv_obj_t *label = lv_group_get_focused(ui_group);
    const char *name = (label && lv_obj_is_valid(label)) ? lv_label_get_text(label) : mac_copy;

    play_sound(SND_INFO_OPEN);
    show_info_box(name, *info_buf ? info_buf : mac_copy, 0);
    msgbox_active = 1;
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
            {ui_lblNavA,      lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.RESCAN, 0},
            {ui_lblNavYGlyph, "",                  0},
            {ui_lblNavY,      lang.MUXBTCON.INFO,  0},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

int muxbtcon_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBTCON.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();

    create_bt_scan_items();
    init_elements();

    init_timer(ui_gen_refresh_task, NULL);
    scan_poll_timer = lv_timer_create(bt_scan_poll_task, 500, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_rescan,
                    [MUX_INPUT_Y] = handle_y,
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
