#include "muxshare.h"
#include "ui/ui_muxbtdev.h"

#define BTDEV(NAME, UDATA) 1,
enum { btdev_count = E_SIZE(BTDEV_ELEMENTS) };
#undef BTDEV

#define BTDEV_INFO(NAME, UDATA) 1,
enum { btdev_info_count = E_SIZE(BTDEV_INFO_ELEMENTS) };
#undef BTDEV_INFO

#define BTDEV_FRIENDLYNAME_IDX 0

#define BTDEV_TYPE_IDX btdev_info_count
#define BTDEV_STAT_IDX (btdev_info_count + 1)
#define BTDEV_FRGT_IDX (btdev_info_count + 2)

static char selected_mac[18];
static int is_connected;
static int type_original;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define BTDEV(NAME, UDATA) {UDATA, lang.muxbtdev.help.NAME},
        BTDEV_ELEMENTS
#undef BTDEV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const char *new_text = lv_textarea_get_text(ui_txt_entry_btdev);

    if (current_item_index == BTDEV_FRIENDLYNAME_IDX) {
        lv_label_set_text(ui_val_friendly_name_btdev, new_text);
        if (*selected_mac) {
            const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "alias", selected_mac, new_text, NULL};
            run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
        }
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txt_entry_btdev, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnl_entry_btdev);
}

static void handle_keyboard_press(void) {
    play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void nav_show_x(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_x, text);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void check_focus(void) {
    if (current_item_index == BTDEV_FRIENDLYNAME_IDX) {
        nav_show_a(1, lang.generic.edit);
        nav_show_lr(0);
        nav_show_x(0, NULL);
    } else if (current_item_index < BTDEV_TYPE_IDX) {
        nav_show_a(0, NULL);
        nav_show_lr(0);
        nav_show_x(0, NULL);
    } else if (current_item_index == BTDEV_TYPE_IDX) {
        nav_show_a(0, NULL);
        nav_show_lr(1);
        nav_show_x(0, NULL);
    } else if (current_item_index == BTDEV_STAT_IDX) {
        nav_show_a(1, is_connected ? lang.muxbtdev.disconnect : lang.muxbtdev.connect);
        nav_show_lr(0);
        nav_show_x(0, NULL);
    } else {
        nav_show_a(0, NULL);
        nav_show_lr(0);
        nav_show_x(1, lang.generic.forget);
    }
}

static int forget_mode = 0;
static mux_dialogue forget_dlg;

static void show_forget_dialog(void) {
    forget_mode = 1;
    forget_dlg.selected = 0;
    dialogue_show(&forget_dlg);
    dialogue_refresh(&forget_dlg, &theme);
}

static void hide_forget_dialog(void) {
    forget_mode = 0;
    dialogue_hide(&forget_dlg);
}

static void do_forget(void) {
    if (!*selected_mac) return;

    play_sound(snd_confirm);

    const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "forget", selected_mac, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    load_mux("btall");
    mux_input_stop();
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, -1, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active || current_item_index != BTDEV_TYPE_IDX) return;
    move_option(ui_dro_type_btdev, -1);
}

static void handle_option_next(void) {
    if (msgbox_active || current_item_index != BTDEV_TYPE_IDX) return;
    move_option(ui_dro_type_btdev, +1);
}

static void save_btdev_options(void) {
    const int current_type = lv_dropdown_get_selected(ui_dro_type_btdev);

    if (!*selected_mac || current_type == type_original) return;

    char mac_clean[18];
    snprintf(mac_clean, sizeof(mac_clean), "%s", selected_mac);
    for (char *p = mac_clean; *p; p++) {
        if (*p == ':') *p = '_';
    }

    char override_path[MAX_BUFFER_SIZE];
    snprintf(override_path, sizeof(override_path), CONF_CONFIG_PATH "bluetooth/type_%s", mac_clean);
    write_text_to_file(override_path, "w", CHAR, bt_type_keys[current_type]);
}

static void status_change(const char *method) {
    lv_refr_now(NULL);

    const char *args[] = {OPT_PATH "script/mux/bt_device.sh", method, selected_mac, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    load_mux("btall");
    mux_input_stop();
}

static void handle_a(void) {
    if (forget_mode) {
        const mux_confirm_opt opt = (mux_confirm_opt) forget_dlg.selected;
        hide_forget_dialog();
        if (opt == mux_confirm_yep) do_forget();
        return;
    }

    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (current_item_index == BTDEV_FRIENDLYNAME_IDX) {
        key_curr = 0;
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);
        lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(num_entry, LV_STATE_DISABLED);
        key_show = 1;

        osk_show(ui_pnl_entry_btdev);
        osk_refresh_labels();
        lv_textarea_set_text(ui_txt_entry_btdev, lv_label_get_text(ui_val_friendly_name_btdev));
        return;
    }

    if (current_item_index <= BTDEV_TYPE_IDX) return;

    if (current_item_index == BTDEV_STAT_IDX) {
        if (!*selected_mac) return;
        if (is_connected) {
            toast_message(lang.muxbtdev.disconnecting, tst_wait_f);
            status_change("disconnect");
        } else {
            toast_message(lang.muxbtdev.connecting, tst_wait_f);
            status_change("connect");
        }
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (forget_mode) {
        hide_forget_dialog();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_btdev);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    save_btdev_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "btall");

    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_btdev);
}

static void handle_x(void) {
    if (key_show) {
        key_show = 0;
        close_osk(key_entry, ui_group, ui_txt_entry_btdev, ui_pnl_entry_btdev);
        return;
    }

    if (msgbox_active || current_item_index != BTDEV_FRGT_IDX || forget_mode) return;

    if (config.settings.advanced.trust_remove) {
        do_forget();
        return;
    }

    play_sound(snd_confirm);
    show_forget_dialog();
}

static void handle_y(void) {
    if (key_show == 1) key_space(ui_txt_entry_btdev);
}

static void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (forget_mode) {
        if (!swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (forget_mode) return;

    handle_list_nav_up_hold();
}

static void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (forget_mode) {
        if (!swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (forget_mode) return;

    handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    if (forget_mode) {
        if (swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_option_prev();
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    if (forget_mode) {
        if (swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_option_next();
}

static void handle_l1(void) {
    if (key_show == 1) {
        key_swap_back();
        return;
    }

    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show == 1) {
        key_swap();
        return;
    }

    if (!key_show) handle_list_nav_page_down();
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_ok_press();
    ev.code == KEY_ESC &&ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txt_entry_btdev);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;
    play_sound(snd_info_open);
    show_help();
}

static void read_device_info_field(const char *key, char *dest) {
    dest[0] = '\0';

    FILE *f = fopen(CONF_CONFIG_PATH "bluetooth/device_info", "r");
    if (!f) return;

    char line[256];
    const size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            const char *val = line + key_len + 1;
            while (*val == ' ')
                val++;
            snprintf(dest, 1024, "%s", val);
            str_remchar(dest, '\n');
            break;
        }
    }
    fclose(f);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[btdev_count];
    static lv_obj_t *ui_objects_value[btdev_count];
    static lv_obj_t *ui_objects_glyph[btdev_count];
    static lv_obj_t *ui_objects_panel[btdev_count];

    char val_name[MAX_BUFFER_SIZE];
    char val_battery[MAX_BUFFER_SIZE];
    char val_icon[MAX_BUFFER_SIZE];
    char val_class[MAX_BUFFER_SIZE];
    char val_uuids[MAX_BUFFER_SIZE];
    char val_conn[MAX_BUFFER_SIZE];

    read_device_info_field("Name", val_name);
    read_device_info_field("Battery", val_battery);
    read_device_info_field("Icon", val_icon);
    read_device_info_field("Class", val_class);
    read_device_info_field("UUIDs", val_uuids);
    read_device_info_field("Connected", val_conn);

    const int connected = strcmp(val_conn, "yes") == 0;
    is_connected = connected;

    const char *name = *val_name ? val_name : lang.generic.unknown;
    const char *battery = *val_battery ? val_battery : lang.generic.unknown;

    bt_type_t type;

    if (*selected_mac) {
        char mac_clean[18];

        snprintf(mac_clean, sizeof(mac_clean), "%s", selected_mac);
        for (char *p = mac_clean; *p; p++) {
            if (*p == ':') *p = '_';
        }

        char override_path[MAX_BUFFER_SIZE];
        snprintf(override_path, sizeof(override_path), CONF_CONFIG_PATH "bluetooth/type_%s", mac_clean);

        char override_key[64] = {0};
        FILE *of = fopen(override_path, "r");

        if (of) {
            if (fgets(override_key, sizeof(override_key), of)) str_remchar(override_key, '\n');
            fclose(of);
        }

        type = *override_key ? bt_type_from_string(override_key)
                             : bt_type_derive(val_icon, val_class, val_uuids, val_name);
    } else {
        type = bt_type_derive(val_icon, val_class, val_uuids, val_name);
    }

    INIT_VALUE_ITEM(-1, btdev, friendly_name, lang.muxbtdev.friendly_name, "friendlyname", name);
    INIT_VALUE_ITEM(-1, btdev, battery, lang.muxbtdev.battery, "battery", battery);
    INIT_VALUE_ITEM(
        -1, btdev, address, lang.muxbtdev.address, "address", *selected_mac ? selected_mac : lang.generic.unknown
    );

    char *type_options[] = {
        lang.muxbtdev.type_name.audio_headset,  lang.muxbtdev.type_name.audio_headphones,
        lang.muxbtdev.type_name.audio_speaker,  lang.muxbtdev.type_name.audio_microphone,
        lang.muxbtdev.type_name.audio_card,     lang.muxbtdev.type_name.input_gamepad,
        lang.muxbtdev.type_name.input_keyboard, lang.muxbtdev.type_name.input_mouse,
        lang.muxbtdev.type_name.input_combo,    lang.muxbtdev.type_name.input_remote,
        lang.muxbtdev.type_name.phone,          lang.muxbtdev.type_name.computer,
        lang.muxbtdev.type_name.network,        lang.muxbtdev.type_name.unknown,
    };

    INIT_OPTION_ITEM(-1, btdev, type, lang.muxbtdev.type, "type", type_options, bt_type_count);
    lv_dropdown_set_selected(ui_dro_type_btdev, type);
    type_original = (int) lv_dropdown_get_selected(ui_dro_type_btdev);

    char *status_options[] = {
        lang.muxbtdev.disconnected,
        lang.muxbtdev.connected,
    };

    INIT_OPTION_ITEM(-1, btdev, status, lang.muxbtdev.status, "status", status_options, 2);
    lv_dropdown_set_selected(ui_dro_status_btdev, connected ? 1 : 0);

    INIT_OPTION_ITEM(-1, btdev, forget, lang.muxbtdev.forget, "forget", NULL, 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.edit, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.forget, 0},
                                  {NULL, NULL, 0}});

#define BTDEV(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_btdev, UDATA);
    BTDEV_ELEMENTS
#undef BTDEV

    check_focus();
    overlay_display();
}

int muxbtdev_main(void) {
    selected_mac[0] = '\0';

    FILE *f = fopen(CONF_CONFIG_PATH "bluetooth/selected", "r");
    if (f) {
        if (fgets(selected_mac, sizeof(selected_mac), f)) str_remchar(selected_mac, '\n');
        fclose(f);
    }

    if (*selected_mac) {
        const char *args[] = {OPT_PATH "script/mux/bt_device.sh", "info", selected_mac, NULL};
        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
    }

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxbtdev.title);
    init_muxbtdev(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnl_entry_btdev, ui_txt_entry_btdev, 1, 0, OSK_MAX);

    dialogue_init_confirm(
        &forget_dlg, &theme, ui_screen, lang.generic.confirm, lang.muxbtdev.forget_confirm, lang.muxbtdev.forget,
        lang.generic.cancel, lang.generic.select, lang.generic.back
    );
    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_left] = handle_left,
            [mux_input_dpad_right] = handle_right,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
