#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/ui/empty_state.h>

static void show_help(void) {
    show_info_box(lang.muxbtall.title, lang.muxbtall.overview, 0);
}

static void check_focus(void);

static int bti_index = -1;

static void list_nav_next(int steps);

static void cancel_bt_poll(void);

static int bt_list_pending = 0;
static time_t bt_list_start = 0;
static lv_timer_t *bt_poll_timer = NULL;

static void populate_paired_device_list(void) {
    FILE *file = fopen(CONF_CONFIG_PATH "bluetooth/paired", "r");
    if (!file) return;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        str_remchar(line, '\n');
        if (strlen(line) == 0) continue;

        char mac[18] = {0};
        int connected = 0;
        char name[64] = {0};

        const char *tok = line;
        char *sp = strchr(tok, ' ');
        if (!sp || sp - tok > 17) continue;
        memcpy(mac, tok, sp - tok);
        mac[sp - tok] = '\0';

        tok = sp + 1;
        char *end;
        const long val = strtol(tok, &end, 10);
        if (end == tok || *end != ' ' || val < 0 || val > 1) continue;
        connected = (int) val;

        snprintf(name, sizeof(name), "%s", end + 1);
        if (name[0] == '\0') continue;

        ui_count_static++;

        lv_obj_t *ui_pnl_device = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_device);

        lv_obj_t *ui_lbl_device = lv_label_create(ui_pnl_device);
        apply_theme_option_item_label(&theme, ui_lbl_device, name, 1);

        lv_obj_t *ui_lbl_device_status = lv_label_create(ui_pnl_device);
        apply_theme_list_value(
            &theme, ui_lbl_device_status, connected ? lang.muxbtall.connected : lang.muxbtall.disconnected
        );

        lv_obj_t *ui_ico_device = lv_img_create(ui_pnl_device);
        apply_theme_list_glyph(&theme, ui_ico_device, mux_module, "bluetooth");

        lv_group_add_obj(ui_group, ui_lbl_device);
        lv_group_add_obj(ui_group_value, ui_lbl_device_status);
        lv_group_add_obj(ui_group_glyph, ui_ico_device);
        lv_group_add_obj(ui_group_panel, ui_pnl_device);

        set_owned_user_data(ui_pnl_device, strdup(mac));
    }
    fclose(file);

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void bt_poll_task(lv_timer_t *t) {
    if (!bt_list_pending) {
        lv_timer_del(t);
        bt_poll_timer = NULL;
        return;
    }

    const int list_timeout =
        config.settings.advanced.bt_scan_timeout > 0 ? config.settings.advanced.bt_scan_timeout : 10;

    struct stat st;
    const int file_ready = stat(CONF_CONFIG_PATH "bluetooth/paired", &st) == 0 && st.st_mtime >= bt_list_start;
    const int timed_out = time(NULL) - bt_list_start >= list_timeout;

    if (!file_ready && !timed_out) return;

    bt_list_pending = 0;
    lv_timer_del(t);
    bt_poll_timer = NULL;

    hide_bounce_progress_bar();
    populate_paired_device_list();

    if (ui_count_static == 0) {
        empty_state_show_action(lang.muxbtall.none, lang.muxbtall.none_hint, "x", lang.generic.scan);
        check_focus();
        return;
    }

    empty_state_hide();

    nav_silent = 1;
    list_nav_next(0);
    nav_silent = 0;

    if (bti_index > 0 && bti_index < ui_count_static) list_nav_next(bti_index);
    bti_index = -1;

    check_focus();
}

static int has_paired_bt_devices(void) {
    struct stat st;
    return stat(CONF_CONFIG_PATH "bluetooth/paired", &st) == 0 && st.st_size > 0;
}

static void create_paired_device_items(void) {
    if (file_exist(MUOS_BTI_LOAD)) {
        bti_index = read_line_int_from(MUOS_BTI_LOAD, 1);
        remove(MUOS_BTI_LOAD);
    }

    if (!has_paired_bt_devices()) {
        empty_state_show_action(lang.muxbtall.none, lang.muxbtall.none_hint, "x", lang.generic.scan);
        return;
    }

    bt_list_start = time(NULL);
    bt_list_pending = 1;

    const int list_timeout =
        config.settings.advanced.bt_scan_timeout > 0 ? config.settings.advanced.bt_scan_timeout : 10;
    show_bounce_progress_bar(lang.muxbtall.loading, list_timeout);

    const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "list", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static const char *get_focused_device_mac(void) {
    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return NULL;

    return lv_obj_get_user_data(panel);
}

static void check_focus(void) {
    nav_show_a(ui_count_static > 0 && lv_group_get_focused(ui_group) != NULL, lang.generic.select);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call || !ui_count_static) return;

    const char *mac = get_focused_device_mac();
    if (!mac) return;

    play_sound(snd_confirm);
    cancel_bt_poll();

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    const char *info_args[] = {OPT_PATH "script/mux/bt_device.sh", "info", mac_copy, NULL};
    run_exec(info_args, A_SIZE(info_args), 0, 1, NULL, NULL);

    write_text_to_file_atomic(CONF_CONFIG_PATH "bluetooth/selected", CHAR, mac_copy);
    write_text_to_file(MUOS_BTI_LOAD, "w", INT, current_item_index);
    load_mux("btdev");
    mux_input_stop();
}

static void cancel_bt_poll(void) {
    if (bt_poll_timer) {
        lv_timer_del(bt_poll_timer);
        bt_poll_timer = NULL;
    }

    bt_list_pending = 0;
    hide_bounce_progress_bar();
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    cancel_bt_poll();
    load_mux("btcon");

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    cancel_bt_poll();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "bluetooth");
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.scan, 0},
                                  {NULL, NULL, 0}});

    check_focus();
    overlay_display();
}

int muxbtall_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbtall.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    reset_ui_groups();
    create_paired_device_items();

    init_timer(ui_gen_refresh_task, NULL);
    bt_poll_timer = lv_timer_create(bt_poll_task, 300, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
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
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);

    orientation_introduce(mux_module, lang.muxbtall.title, lang.muxbtall.overview);

    mux_input_task(&input_opts);

    return 0;
}
