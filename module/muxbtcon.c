#include "muxshare.h"

static void cancel_scan(void);

static time_t scan_start = 0;
static time_t last_scan_mtime = 0;
static lv_timer_t *scan_poll_timer = NULL;

static void show_help(void) {
    show_info_box(lang.muxbtcon.title, lang.muxbtcon.help, 0);
}

static void populate_bt_scan_items(void) {
    reset_ui_groups();

    const char *scan_file = CONF_CONFIG_PATH "bluetooth/scan";
    FILE *file = fopen(scan_file, "r");
    if (!file) return;

    char line[96];
    while (fgets(line, sizeof(line), file)) {
        str_remchar(line, '\n');
        if (strlen(line) == 0) continue;

        char mac[18] = {0};
        char name[64] = {0};
        if (sscanf(line, "%17s %63[^\n]", mac, name) < 2) continue;

        ui_count_static++;

        lv_obj_t *ui_pnl_bt_scan = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_bt_scan);
        lv_obj_set_user_data(ui_pnl_bt_scan, strdup(mac));

        lv_obj_t *ui_lbl_bt_scan_item = lv_label_create(ui_pnl_bt_scan);
        apply_theme_list_item(&theme, ui_lbl_bt_scan_item, name);

        lv_obj_t *ui_lbl_bt_scan_glyph = lv_img_create(ui_pnl_bt_scan);
        apply_theme_list_glyph(&theme, ui_lbl_bt_scan_glyph, mux_module, "bluetooth");

        lv_group_add_obj(ui_group, ui_lbl_bt_scan_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_bt_scan_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_bt_scan);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_bt_scan_item, ui_lbl_bt_scan_glyph, name);
        apply_text_long_dot(&theme, ui_lbl_bt_scan_item);
    }

    fclose(file);

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void refresh_scan_list(void) {
    const uint32_t child_cnt = lv_obj_get_child_cnt(ui_pnl_content);
    for (uint32_t i = 0; i < child_cnt; i++) {
        free(lv_obj_get_user_data(lv_obj_get_child(ui_pnl_content, (int32_t) i)));
    }

    lv_obj_clean(ui_pnl_content);

    if (ui_group) lv_group_del(ui_group);
    if (ui_group_glyph) lv_group_del(ui_group_glyph);
    if (ui_group_panel) lv_group_del(ui_group_panel);
    ui_group = ui_group_glyph = ui_group_panel = NULL;
    ui_count_static = current_item_index = 0;

    reset_ui_groups();
    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);

    populate_bt_scan_items();

    lv_label_set_text(ui_lbl_screen_message, !ui_count_static ? lang.muxbtcon.none : "");

    if (ui_count_static > 0) {
        nav_silent = 1;
        gen_step_movement(0, +1, 1, 0, 1);
        nav_silent = 0;
    }
}

static void bt_scan_poll_task(lv_timer_t *timer __attribute__((unused))) {
    struct stat st;
    if (stat(CONF_CONFIG_PATH "bluetooth/scan", &st) != 0) return;

    const time_t mtime = st.st_mtime;
    if (mtime < scan_start || mtime <= last_scan_mtime) return;

    last_scan_mtime = mtime;
    refresh_scan_list();
}

static void create_bt_scan_items(void) {
    lv_label_set_text(ui_lbl_screen_message, lang.muxbtcon.scan);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    scan_start = time(NULL);
    last_scan_mtime = 0;

    const char *args[] = {OPT_PATH "script/mux/bt_scan.sh", "list", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static void cancel_scan(void) {
    const char *args[] = {OPT_PATH "script/mux/bt_scan.sh", "stop", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *mac = lv_obj_get_user_data(panel);
    if (!mac) return;

    play_sound(snd_confirm);
    cancel_scan();

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    toast_message(lang.muxbtcon.connect, tst_wait_f);

    const char *args[] = {OPT_PATH "script/mux/bt_scan.sh", "connect", mac_copy, NULL};
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

    play_sound(snd_back);
    cancel_scan();

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || !ui_count_static) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *mac = lv_obj_get_user_data(panel);
    if (!mac) return;

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    toast_message(lang.generic.refresh_run, tst_wait_f);

    const char *args[] = {OPT_PATH "script/mux/bt_scan.sh", "info", mac_copy, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    lv_obj_set_style_opa(ui_pnl_message, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    char info_buf[512] = {0};
    FILE *f = fopen("/run/muos/bt_info", "r");
    if (f) {
        __attribute__((unused)) size_t r = fread(info_buf, 1, sizeof(info_buf) - 1, f);
        fclose(f);
    }

    const lv_obj_t *label = lv_group_get_focused(ui_group);
    const char *name = label && lv_obj_is_valid(label) ? lv_label_get_text(label) : mac_copy;

    play_sound(snd_info_open);
    show_info_box(name, *info_buf ? info_buf : mac_copy, 0);
    msgbox_active = 1;
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.select, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.muxbtcon.info, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxbtcon_main(void) {
    scan_start = 0;
    last_scan_mtime = 0;
    scan_poll_timer = NULL;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbtcon.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();

    create_bt_scan_items();
    init_elements();

    init_timer(ui_gen_refresh_task, NULL);
    scan_poll_timer = lv_timer_create(bt_scan_poll_task, 500, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_y] = handle_y,
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
