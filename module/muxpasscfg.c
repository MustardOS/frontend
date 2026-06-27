#include "muxshare.h"
#include "ui/ui_muxpasscfg.h"

static struct mux_passcode passcfg;

#define PASSCFG(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(PASSCFG_ELEMENTS) };
#undef PASSCFG

static int edit_field = -1;

static int is_code_field(const int idx) {
    return idx == 0 || idx == 2 || idx == 4 || idx == 6;
}

static const char *code_display(const char *code) {
    return strcasecmp(code, "000000") == 0 ? lang.generic.disabled : code;
}

static const char *msg_display(const char *msg) {
    return msg && msg[0] ? msg : "-";
}

static void update_value_labels(void) {
    lv_label_set_text(ui_val_boot_code_passcfg, code_display(passcfg.code.boot));
    lv_label_set_text(ui_val_boot_msg_passcfg, msg_display(passcfg.message.boot));
    lv_label_set_text(ui_val_launch_code_passcfg, code_display(passcfg.code.launch));
    lv_label_set_text(ui_val_launch_msg_passcfg, msg_display(passcfg.message.launch));
    lv_label_set_text(ui_val_setting_code_passcfg, code_display(passcfg.code.setting));
    lv_label_set_text(ui_val_setting_msg_passcfg, msg_display(passcfg.message.setting));
    lv_label_set_text(ui_val_safety_code_passcfg, code_display(passcfg.code.safety));
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define PASSCFG(NAME, UDATA) {UDATA, lang.muxpasscfg.help.NAME},
        PASSCFG_ELEMENTS
#undef PASSCFG
    };
    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void open_entry(const int idx) {
    edit_field = idx;

    if (is_code_field(idx)) {
        lv_textarea_set_max_length(ui_txt_entry_passcfg, 6);
        lv_textarea_set_password_mode(ui_txt_entry_passcfg, 0);

        lv_obj_clear_flag(hex_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(hex_entry, LV_STATE_DISABLED);
        lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(key_entry, LV_STATE_DISABLED);
        key_show = 3;
    } else {
        lv_textarea_set_max_length(ui_txt_entry_passcfg, OSK_MAX);
        lv_textarea_set_password_mode(ui_txt_entry_passcfg, 0);

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);
        if (hex_entry && lv_obj_is_valid(hex_entry)) {
            lv_obj_add_flag(hex_entry, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(hex_entry, LV_STATE_DISABLED);
        }
        key_show = 1;
    }

    const char *current = "";
    switch (idx) {
        case 0:
            current = passcfg.code.boot;
            break;
        case 1:
            current = passcfg.message.boot;
            break;
        case 2:
            current = passcfg.code.launch;
            break;
        case 3:
            current = passcfg.message.launch;
            break;
        case 4:
            current = passcfg.code.setting;
            break;
        case 5:
            current = passcfg.message.setting;
            break;
        case 6:
            current = passcfg.code.safety;
            break;
        default:
            break;
    }

    if (is_code_field(idx) && strcasecmp(current, "000000") == 0) current = "";

    lv_textarea_set_text(ui_txt_entry_passcfg, current);
    osk_show(ui_pnl_entry_passcfg);
}

static void validate_and_save(void) {
    const char *raw = lv_textarea_get_text(ui_txt_entry_passcfg);

    if (is_code_field(edit_field)) {
        const size_t len = strlen(raw);
        for (size_t i = 0; i < len; i++) {
            const char c = (char) toupper((unsigned char) raw[i]);
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                play_sound(snd_error);
                toast_message(lang.muxpasscfg.invalid, tst_wait_s);
                return;
            }
        }

        char code[7] = "000000";
        if (len > 0 && len <= 6) {
            const int pad = 6 - (int) len;
            for (int i = 0; i < pad; i++)
                code[i] = '0';
            memcpy(code + pad, raw, len);
            code[6] = '\0';
        }

        switch (edit_field) {
            case 0:
                snprintf(passcfg.code.boot, MAX_BUFFER_SIZE, "%s", code);
                break;
            case 2:
                snprintf(passcfg.code.launch, MAX_BUFFER_SIZE, "%s", code);
                break;
            case 4:
                snprintf(passcfg.code.setting, MAX_BUFFER_SIZE, "%s", code);
                break;
            case 6:
                snprintf(passcfg.code.safety, MAX_BUFFER_SIZE, "%s", code);
                break;
            default:
                break;
        }
    } else {
        switch (edit_field) {
            case 1:
                snprintf(passcfg.message.boot, MAX_BUFFER_SIZE, "%s", raw);
                break;
            case 3:
                snprintf(passcfg.message.launch, MAX_BUFFER_SIZE, "%s", raw);
                break;
            case 5:
                snprintf(passcfg.message.setting, MAX_BUFFER_SIZE, "%s", raw);
                break;
            default:
                break;
        }
    }

    save_passcode(&passcfg);
    update_value_labels();
    play_sound(snd_confirm);
    toast_message(lang.muxpasscfg.saved, tst_wait_s);
}

static void handle_keyboard_ok_press(void) {
    validate_and_save();

    const int was_code = is_code_field(edit_field);
    key_show = 0;
    edit_field = -1;

    reset_osk(was_code ? hex_entry : key_entry);
    lv_textarea_set_text(ui_txt_entry_passcfg, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_passcfg);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    lv_obj_t *active = key_show == 3 ? hex_entry : key_entry;
    const char *is_key = lv_btnmatrix_get_btn_text(active, key_curr);

    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(active, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    open_entry(current_item_index);
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_passcfg);
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "lock");
    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_passcfg);
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        key_show = 0;
        edit_field = -1;
        lv_textarea_set_text(ui_txt_entry_passcfg, "");
        lv_group_set_focus_cb(ui_group, NULL);
        osk_hide(ui_pnl_entry_passcfg);
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || key_show || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    key_show ? key_up() : handle_list_nav_up();
}

static void handle_up_hold(void) {
    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
    key_show ? key_down() : handle_list_nav_down();
}

static void handle_down_hold(void) {
    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (key_show) key_left();
}

static void handle_right(void) {
    if (key_show) key_right();
}

static void handle_left_hold(void) {
    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (key_show) key_right();
}

static void handle_l1(void) {
    if (key_show) return;

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) return;

    handle_list_nav_page_down();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, passcfg, boot_code, lang.muxpasscfg.bootcode, "boot_lock", code_display(passcfg.code.boot));
    INIT_VALUE_ITEM(-1, passcfg, boot_msg, lang.muxpasscfg.bootmsg, "boot_info", msg_display(passcfg.message.boot));
    INIT_VALUE_ITEM(
        -1, passcfg, launch_code, lang.muxpasscfg.launchcode, "launch_lock", code_display(passcfg.code.launch)
    );
    INIT_VALUE_ITEM(
        -1, passcfg, launch_msg, lang.muxpasscfg.launchmsg, "launch_info", msg_display(passcfg.message.launch)
    );
    INIT_VALUE_ITEM(
        -1, passcfg, setting_code, lang.muxpasscfg.settingcode, "setting_lock", code_display(passcfg.code.setting)
    );
    INIT_VALUE_ITEM(
        -1, passcfg, setting_msg, lang.muxpasscfg.settingmsg, "setting_info", msg_display(passcfg.message.setting)
    );
    INIT_VALUE_ITEM(-1, passcfg, safety_code, lang.muxpasscfg.safetycode, "safety", code_display(passcfg.code.safety));

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_ok_press();

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txt_entry_passcfg);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.edit, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxpasscfg_main(void) {
    init_module(__func__);
    load_passcode(&passcfg);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxpasscfg.title);
    init_muxpasscfg(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnl_entry_passcfg, ui_txt_entry_passcfg, 2, 0, 6);
    if (hex_entry && lv_obj_is_valid(hex_entry)) {
        lv_obj_add_flag(hex_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(hex_entry, LV_STATE_DISABLED);
    }

    init_timer(NULL, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
