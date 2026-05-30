#include "muxshare.h"
#include "ui/ui_muxbtdev.h"

#define BTDEV(NAME, ENUM, UDATA) 1,
enum {
    BTDEV_COUNT = E_SIZE(BTDEV_ELEMENTS)
};
#undef BTDEV

#define BTDEV_INFO(NAME, ENUM, UDATA) 1,
enum {
    BTDEV_INFO_COUNT = E_SIZE(BTDEV_INFO_ELEMENTS)
};
#undef BTDEV_INFO

#define BTDEV_FRIENDLYNAME_IDX 0

#define BTDEV_TYPE_IDX BTDEV_INFO_COUNT
#define BTDEV_STAT_IDX (BTDEV_INFO_COUNT + 1)
#define BTDEV_FRGT_IDX (BTDEV_INFO_COUNT + 2)

static char selected_mac[18];
static int is_connected;
static int type_original;

static void show_help(void) {
    struct help_msg help_messages[] = {
#define BTDEV(NAME, ENUM, UDATA) { UDATA, lang.MUXBTDEV.HELP.ENUM },
            BTDEV_ELEMENTS
#undef BTDEV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    const char *new_text = lv_textarea_get_text(ui_txtEntry_btdev);

    if (current_item_index == BTDEV_FRIENDLYNAME_IDX) {
        lv_label_set_text(ui_lblFriendlyNameValue_btdev, new_text);
        if (*selected_mac) {
            const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "alias", selected_mac, new_text, NULL};
            run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
        }
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_btdev, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnlEntry_btdev);
}

static void handle_keyboard_press(void) {
    play_sound(SND_KEYPRESS);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void nav_show_a(int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lblNavA, text);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_lr(int show) {
    if (show) {
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_x(int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lblNavX, text);
        lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void check_focus(void) {
    if (current_item_index == BTDEV_FRIENDLYNAME_IDX) {
        nav_show_a(1, lang.GENERIC.EDIT);
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
        nav_show_a(1, is_connected ? lang.MUXBTALL.DISCONNECT : lang.MUXBTALL.CONNECT);
        nav_show_lr(0);
        nav_show_x(0, NULL);
    } else {
        nav_show_a(0, NULL);
        nav_show_lr(0);
        nav_show_x(1, lang.GENERIC.REMOVE);
    }
}

static int forget_mode = 0;
static int skip_confirm = 0;
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

    play_sound(SND_CONFIRM);

    const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "forget", selected_mac, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    load_mux("btall");
    mux_input_stop();
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, -1);
    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active || current_item_index != BTDEV_TYPE_IDX) return;
    move_option(ui_droType_btdev, -1);
}

static void handle_option_next(void) {
    if (msgbox_active || current_item_index != BTDEV_TYPE_IDX) return;
    move_option(ui_droType_btdev, +1);
}

static void save_btdev_options(void) {
    int current_type = (int) lv_dropdown_get_selected(ui_droType_btdev);

    if (!*selected_mac || current_type == type_original) return;

    char mac_clean[18];
    snprintf(mac_clean, sizeof(mac_clean), "%s", selected_mac);
    for (char *p = mac_clean; *p; p++) { if (*p == ':') *p = '_'; }

    char override_path[MAX_BUFFER_SIZE];
    snprintf(override_path, sizeof(override_path), CONF_CONFIG_PATH "bluetooth/type_%s", mac_clean);
    write_text_to_file(override_path, "w", CHAR, bt_type_keys[current_type]);
}

static void status_change(const char *method) {
    lv_refr_now(NULL);

    const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), method, selected_mac, NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    load_mux("btall");
    mux_input_stop();
}

static void handle_a(void) {
    if (forget_mode) {
        mux_remove_opt opt = (mux_remove_opt) forget_dlg.selected;
        hide_forget_dialog();
        if (opt == MUX_REMOVE_YEP) {
            do_forget();
        } else if (opt == MUX_REMOVE_SKIP) {
            skip_confirm = 1;
            do_forget();
        }
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

        osk_show(ui_pnlEntry_btdev);
        osk_refresh_labels();
        lv_textarea_set_text(ui_txtEntry_btdev, lv_label_get_text(ui_lblFriendlyNameValue_btdev));
        return;
    }

    if (current_item_index <= BTDEV_TYPE_IDX) return;

    if (current_item_index == BTDEV_STAT_IDX) {
        if (!*selected_mac) return;
        if (is_connected) {
            toast_message(lang.MUXBTCON.DISCONNECT, FOREVER);
            status_change("disconnect");
        } else {
            toast_message(lang.MUXBTCON.CONNECT, FOREVER);
            status_change("connect");
        }
        return;
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (forget_mode) {
        hide_forget_dialog();
        return;
    }

    if (key_show) {
        key_backspace(ui_txtEntry_btdev);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(SND_BACK);

    save_btdev_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "btall");

    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txtEntry_btdev);
}

static void handle_x(void) {
    if (key_show) {
        key_show = 0;
        close_osk(key_entry, ui_group, ui_txtEntry_btdev, ui_pnlEntry_btdev);
        return;
    }

    if (msgbox_active || current_item_index != BTDEV_FRGT_IDX || forget_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE || skip_confirm) {
        do_forget();
        return;
    }

    play_sound(SND_CONFIRM);
    show_forget_dialog();
}

static void handle_y(void) {
    if (key_show == 1) key_space(ui_txtEntry_btdev);
}

static void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (forget_mode) {
        if (!swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
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
            play_sound(SND_NAVIGATE);
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
            play_sound(SND_NAVIGATE);
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
            play_sound(SND_NAVIGATE);
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

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_OK_press();
    ev.code == KEY_ESC && ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txtEntry_btdev);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;
    play_sound(SND_INFO_OPEN);
    show_help();
}

static void read_device_info_field(const char *key, char *dest) {
    dest[0] = '\0';

    FILE *f = fopen(CONF_CONFIG_PATH "bluetooth/device_info", "r");
    if (!f) return;

    char line[256];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            const char *val = line + key_len + 1;
            while (*val == ' ') val++;
            snprintf(dest, 1024, "%s", val);
            str_remchar(dest, '\n');
            break;
        }
    }
    fclose(f);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[BTDEV_COUNT];
    static lv_obj_t *ui_objects_value[BTDEV_COUNT];
    static lv_obj_t *ui_objects_glyph[BTDEV_COUNT];
    static lv_obj_t *ui_objects_panel[BTDEV_COUNT];

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

    int connected = (strcmp(val_conn, "yes") == 0);
    is_connected = connected;

    const char *name = *val_name ? val_name : lang.GENERIC.UNKNOWN;
    const char *battery = *val_battery ? val_battery : lang.GENERIC.UNKNOWN;

    bt_type_t type;

    if (*selected_mac) {
        char mac_clean[18];

        snprintf(mac_clean, sizeof(mac_clean), "%s", selected_mac);
        for (char *p = mac_clean; *p; p++) { if (*p == ':') *p = '_'; }

        char override_path[MAX_BUFFER_SIZE];
        snprintf(override_path, sizeof(override_path), CONF_CONFIG_PATH "bluetooth/type_%s", mac_clean);

        char override_key[64] = {0};
        FILE *of = fopen(override_path, "r");

        if (of) {
            if (fgets(override_key, sizeof(override_key), of)) str_remchar(override_key, '\n');
            fclose(of);
        }

        type = *override_key ? bt_type_from_string(override_key) : bt_type_derive(val_icon, val_class, val_uuids, val_name);
    } else {
        type = bt_type_derive(val_icon, val_class, val_uuids, val_name);
    }

    INIT_VALUE_ITEM(-1, btdev, FriendlyName, lang.MUXBTDEV.FRIENDLYNAME, "friendlyname", name);
    INIT_VALUE_ITEM(-1, btdev, Battery, lang.MUXBTDEV.BATTERY, "battery", battery);
    INIT_VALUE_ITEM(-1, btdev, Address, lang.MUXBTDEV.ADDRESS, "address", *selected_mac ? selected_mac : lang.GENERIC.UNKNOWN);

    char *type_options[] = {
            lang.MUXBTDEV.TYPE_NAME.AUDIO_HEADSET,
            lang.MUXBTDEV.TYPE_NAME.AUDIO_HEADPHONES,
            lang.MUXBTDEV.TYPE_NAME.AUDIO_SPEAKER,
            lang.MUXBTDEV.TYPE_NAME.AUDIO_MICROPHONE,
            lang.MUXBTDEV.TYPE_NAME.AUDIO_CARD,
            lang.MUXBTDEV.TYPE_NAME.INPUT_GAMEPAD,
            lang.MUXBTDEV.TYPE_NAME.INPUT_KEYBOARD,
            lang.MUXBTDEV.TYPE_NAME.INPUT_MOUSE,
            lang.MUXBTDEV.TYPE_NAME.INPUT_COMBO,
            lang.MUXBTDEV.TYPE_NAME.INPUT_REMOTE,
            lang.MUXBTDEV.TYPE_NAME.PHONE,
            lang.MUXBTDEV.TYPE_NAME.COMPUTER,
            lang.MUXBTDEV.TYPE_NAME.NETWORK,
            lang.MUXBTDEV.TYPE_NAME.UNKNOWN,
    };

    INIT_OPTION_ITEM(-1, btdev, Type, lang.MUXBTDEV.TYPE, "type", type_options, BT_TYPE_COUNT);
    lv_dropdown_set_selected(ui_droType_btdev, (int) type);
    type_original = (int) lv_dropdown_get_selected(ui_droType_btdev);

    char *status_options[] = {
            lang.MUXBTDEV.DISCONNECTED,
            lang.MUXBTDEV.CONNECTED,
    };

    INIT_OPTION_ITEM(-1, btdev, Status, lang.MUXBTDEV.STATUS, "status", status_options, 2);
    lv_dropdown_set_selected(ui_droStatus_btdev, connected ? 1 : 0);

    INIT_OPTION_ITEM(-1, btdev, Forget, lang.MUXBTDEV.FORGET, "forget", NULL, 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.EDIT,   0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.REMOVE, 0},
            {NULL, NULL,                            0}
    });

#define BTDEV(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_btdev, UDATA);
    BTDEV_ELEMENTS
#undef BTDEV

    check_focus();
    overlay_display();
}

int muxbtdev_main(void) {
    selected_mac[0] = '\0';

    FILE *f = fopen(CONF_CONFIG_PATH "bluetooth/selected", "r");
    if (f) {
        __attribute__((unused)) char *_r = fgets(selected_mac, sizeof(selected_mac), f);
        fclose(f);
        str_remchar(selected_mac, '\n');
    }

    if (*selected_mac) {
        const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "info", selected_mac, NULL};
        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
    }

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBTDEV.TITLE);
    init_muxbtdev(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnlEntry_btdev, ui_txtEntry_btdev, true, false, OSK_MAX);

    const char *forget_opts[MUX_REMOVE_CNT] = {lang.MUXBTDEV.FORGET, lang.GENERIC.SKIP_CONFIRM, lang.GENERIC.CANCEL};
    dialogue_init(&forget_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, forget_opts, MUX_REMOVE_CNT, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_X]          = handle_x,
                    [MUX_INPUT_Y]          = handle_y,
                    [MUX_INPUT_DPAD_LEFT]  = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_DPAD_UP]    = handle_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_down,
                    [MUX_INPUT_L1]         = handle_l1,
                    [MUX_INPUT_R1]         = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_L2]   = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_B]          = handle_b_hold,
                    [MUX_INPUT_DPAD_LEFT]  = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_DPAD_UP]    = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN]  = handle_down_hold,
                    [MUX_INPUT_L1]         = handle_l1,
                    [MUX_INPUT_L2]         = hold_call_set,
                    [MUX_INPUT_R1]         = handle_r1,
            },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
