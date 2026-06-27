#include "muxshare.h"
#include "ui/ui_muxbtall.h"

#define BTALL(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(BTALL_ELEMENTS) };
#undef BTALL

#define BTALL(NAME, UDATA) static int NAME##_original;
BTALL_ELEMENTS
#undef BTALL

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define BTALL(NAME, UDATA) {UDATA, lang.muxbtall.help.NAME},
        BTALL_ELEMENTS
#undef BTALL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define BTALL(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_btall);
    BTALL_ELEMENTS
#undef BTALL
}

static void restore_btall_options(void) {
    lv_dropdown_set_selected(ui_dro_auto_connect_btall, config.bluetooth.auto_connect);
}

static void save_btall_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(btall, auto_connect, "bluetooth/autoconnect", INT, 0);

    if (is_modified > 0) {
        const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "autoconnect", NULL};
        run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
        refresh_config = 1;
    }
}

static void check_focus(void);

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
        apply_theme_list_item(&theme, ui_lbl_device, name);

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

        lv_obj_set_user_data(ui_pnl_device, strdup(mac));

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_device, ui_ico_device, name);
        apply_text_long_dot(&theme, ui_lbl_device);
    }
    fclose(file);

    if (ui_count_static > ui_count_dynamic) lv_obj_update_layout(ui_pnl_content);
}

static void bt_poll_task(lv_timer_t *t) {
    if (!bt_list_pending) {
        lv_timer_del(t);
        bt_poll_timer = NULL;
        return;
    }

    struct stat st;
    const int file_ready = stat(CONF_CONFIG_PATH "bluetooth/paired", &st) == 0;
    const int timed_out = time(NULL) - bt_list_start >= 10;

    if (!file_ready && !timed_out) return;

    bt_list_pending = 0;
    lv_timer_del(t);
    bt_poll_timer = NULL;

    populate_paired_device_list();

    lv_label_set_text(ui_lbl_screen_message, ui_count_static <= ui_count_dynamic ? lang.muxbtall.none : "");

    if (ui_count_static > ui_count_dynamic) {
        nav_silent = 1;
        list_nav_next(0);
        nav_silent = 0;
        check_focus();
    }
}

static void create_paired_device_items(void) {
    lv_label_set_text(ui_lbl_screen_message, lang.muxbtall.loading);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    remove(CONF_CONFIG_PATH "bluetooth/paired");

    bt_list_start = time(NULL);
    bt_list_pending = 1;

    const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "list", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static const char *get_focused_device_mac(void) {
    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return NULL;

    return lv_obj_get_user_data(panel);
}

static void nav_show_a(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_a, text);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_lr(const int show) {
    if (show) {
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_auto_connect_btall) {
        nav_show_a(0, NULL);
        nav_show_lr(1);
    } else if (e_focused) {
        nav_show_a(1, lang.generic.select);
        nav_show_lr(0);
    } else {
        nav_show_a(0, NULL);
        nav_show_lr(0);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active || current_item_index >= ui_count_dynamic) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || current_item_index >= ui_count_dynamic) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_general = 0,
        menu_option,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {NULL, &kiosk_pass, menu_option}, // Auto Connect
    };

    if (current_item_index < ui_count_dynamic) {
        const menu_entry *entry = &entries[current_item_index];
        if (entry->action == menu_general) {
            play_sound(snd_confirm);
            cancel_bt_poll();
            save_btall_options();
            load_mux(entry->mux_name);
            mux_input_stop();
        } else {
            handle_option_next();
        }
        return;
    }

    const char *mac = get_focused_device_mac();
    if (!mac) return;

    play_sound(snd_confirm);
    cancel_bt_poll();

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    const char *info_args[] = {OPT_PATH "script/mux/bt_device.sh", "info", mac_copy, NULL};
    run_exec(info_args, A_SIZE(info_args), 0, 1, NULL, NULL);

    write_text_to_file(CONF_CONFIG_PATH "bluetooth/selected", "w", CHAR, mac_copy);
    load_mux("btdev");
    mux_input_stop();
}

static void cancel_bt_poll(void) {
    if (bt_poll_timer) {
        lv_timer_del(bt_poll_timer);
        bt_poll_timer = NULL;
    }

    bt_list_pending = 0;
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    cancel_bt_poll();
    save_btall_options();
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
    save_btall_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "bluetooth");
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    if (current_item_index >= ui_count_dynamic) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *auto_connect_options[] = {lang.generic.disabled, lang.generic.enabled};

    INIT_OPTION_ITEM(-1, btall, auto_connect, lang.muxbtall.auto_connect, "autoconnect", auto_connect_options, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 1);

    create_paired_device_items();
    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.scan, 0},
                                  {NULL, NULL, 0}});

    check_focus();

#define BTALL(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_btall, UDATA);
    BTALL_ELEMENTS
#undef BTALL

    overlay_display();
}

int muxbtall_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbtall.title);
    init_muxbtall(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_btall_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    bt_poll_timer = lv_timer_create(bt_poll_task, 300, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
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
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);

    mux_input_task(&input_opts);

    return 0;
}
