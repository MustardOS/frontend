#include "muxshare.h"

static void cancel_scan(void);

static int scan_pending = 0;
static time_t scan_start = 0;
static lv_timer_t *scan_poll_timer = NULL;

#define NET_SCAN_TIMEOUT 30

static void show_help(void) {
    show_info_box(lang.muxnetscan.title, lang.muxnetscan.help, 0);
}

static void populate_network_items(void) {
    reset_ui_groups();

    const char *scan_file = "/tmp/net_scan";
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

        ui_count_static++;

        lv_obj_t *ui_pnl_net_scan = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_net_scan);
        lv_obj_set_user_data(ui_pnl_net_scan, strdup(str_nonew(ssid)));

        lv_obj_t *ui_lbl_net_scan_item = lv_label_create(ui_pnl_net_scan);
        apply_theme_list_item(&theme, ui_lbl_net_scan_item, str_nonew(ssid));

        lv_obj_t *ui_lbl_net_scan_glyph = lv_img_create(ui_pnl_net_scan);
        apply_theme_list_glyph(&theme, ui_lbl_net_scan_glyph, mux_module, "netscan");

        lv_group_add_obj(ui_group, ui_lbl_net_scan_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_net_scan_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_net_scan);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_net_scan_item, ui_lbl_net_scan_glyph, str_nonew(ssid));
        apply_text_long_dot(&theme, ui_lbl_net_scan_item);
    }
    fclose(file);

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void net_scan_poll_task(lv_timer_t *t) {
    if (!scan_pending) {
        lv_timer_del(t);
        scan_poll_timer = NULL;
        return;
    }

    struct stat st;
    const int file_ready = stat("/tmp/net_scan", &st) == 0;
    const int elapsed = (int) (time(NULL) - scan_start);
    const int timed_out = elapsed >= NET_SCAN_TIMEOUT;

    if (!file_ready && !timed_out) return;

    scan_pending = 0;
    lv_timer_del(t);
    scan_poll_timer = NULL;

    hide_bounce_progress_bar();
    populate_network_items();

    lv_label_set_text(ui_lbl_screen_message, !ui_count_static ? lang.muxnetscan.none : "");

    if (ui_count_static > 0) gen_step_movement(0, +1, 1, 0, 0);
}

static void create_network_items(void) {
    remove("/tmp/net_scan");

    scan_start = time(NULL);
    scan_pending = 1;

    show_bounce_progress_bar(lang.muxnetscan.scan);

    const char *args[] = {OPT_PATH "script/web/ssid.sh", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static void cancel_scan(void) {
    if (scan_poll_timer) {
        lv_timer_del(scan_poll_timer);
        scan_poll_timer = NULL;
    }

    scan_pending = 0;
    hide_bounce_progress_bar();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
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

    play_sound(snd_back);
    cancel_scan();

    mux_input_stop();
}

static void handle_rescan(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    cancel_scan();
    load_mux("net_scan");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.use, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.rescan, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxnetscan_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxnetscan.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();

    create_network_items();
    init_elements();

    init_timer(ui_gen_refresh_task, NULL);
    scan_poll_timer = lv_timer_create(net_scan_poll_task, 500, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_rescan,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
