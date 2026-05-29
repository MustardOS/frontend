#include "muxshare.h"
#include "ui/ui_muxpasscfg.h"

static struct mux_passcode passcfg;

#define PASSCFG(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(PASSCFG_ELEMENTS)
};
#undef PASSCFG

static int edit_field = -1;

static int is_code_field(int idx) {
    return idx == 0 || idx == 2 || idx == 4 || idx == 6;
}

static const char *code_display(const char *code) {
    return (strcasecmp(code, "000000") == 0) ? lang.GENERIC.DISABLED : code;
}

static const char *msg_display(const char *msg) {
    return (msg && msg[0]) ? msg : "-";
}

static void update_value_labels(void) {
    lv_label_set_text(ui_lblBootCodeValue_passcfg, code_display(passcfg.CODE.BOOT));
    lv_label_set_text(ui_lblBootMsgValue_passcfg, msg_display(passcfg.MESSAGE.BOOT));
    lv_label_set_text(ui_lblLaunchCodeValue_passcfg, code_display(passcfg.CODE.LAUNCH));
    lv_label_set_text(ui_lblLaunchMsgValue_passcfg, msg_display(passcfg.MESSAGE.LAUNCH));
    lv_label_set_text(ui_lblSettingCodeValue_passcfg, code_display(passcfg.CODE.SETTING));
    lv_label_set_text(ui_lblSettingMsgValue_passcfg, msg_display(passcfg.MESSAGE.SETTING));
    lv_label_set_text(ui_lblSafetyCodeValue_passcfg, code_display(passcfg.CODE.SAFETY));
}

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define PASSCFG(NAME, ENUM, UDATA) { UDATA, lang.MUXPASSCFG.HELP.ENUM },
            PASSCFG_ELEMENTS
#undef PASSCFG
    };
    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void open_entry(int idx) {
    edit_field = idx;

    if (is_code_field(idx)) {
        lv_textarea_set_max_length(ui_txtEntry_passcfg, 6);
        lv_textarea_set_password_mode(ui_txtEntry_passcfg, 0);

        lv_obj_clear_flag(hex_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(hex_entry, LV_STATE_DISABLED);
        lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(key_entry, LV_STATE_DISABLED);
        key_show = 3;
    } else {
        lv_textarea_set_max_length(ui_txtEntry_passcfg, OSK_MAX);
        lv_textarea_set_password_mode(ui_txtEntry_passcfg, 0);

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
            current = passcfg.CODE.BOOT;
            break;
        case 1:
            current = passcfg.MESSAGE.BOOT;
            break;
        case 2:
            current = passcfg.CODE.LAUNCH;
            break;
        case 3:
            current = passcfg.MESSAGE.LAUNCH;
            break;
        case 4:
            current = passcfg.CODE.SETTING;
            break;
        case 5:
            current = passcfg.MESSAGE.SETTING;
            break;
        case 6:
            current = passcfg.CODE.SAFETY;
            break;
        default:
            break;
    }

    if (is_code_field(idx) && strcasecmp(current, "000000") == 0) current = "";

    lv_textarea_set_text(ui_txtEntry_passcfg, current);
    osk_show(ui_pnlEntry_passcfg);
}

static void validate_and_save(void) {
    const char *raw = lv_textarea_get_text(ui_txtEntry_passcfg);

    if (is_code_field(edit_field)) {
        size_t len = strlen(raw);
        for (size_t i = 0; i < len; i++) {
            char c = (char) toupper((unsigned char) raw[i]);
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                play_sound(SND_ERROR);
                toast_message(lang.MUXPASSCFG.INVALID, SHORT);
                return;
            }
        }

        char code[7] = "000000";
        if (len > 0 && len <= 6) {
            int pad = 6 - (int) len;
            for (int i = 0; i < pad; i++) code[i] = '0';
            memcpy(code + pad, raw, len);
            code[6] = '\0';
        }

        switch (edit_field) {
            case 0:
                strncpy(passcfg.CODE.BOOT, code, MAX_BUFFER_SIZE - 1);
                break;
            case 2:
                strncpy(passcfg.CODE.LAUNCH, code, MAX_BUFFER_SIZE - 1);
                break;
            case 4:
                strncpy(passcfg.CODE.SETTING, code, MAX_BUFFER_SIZE - 1);
                break;
            case 6:
                strncpy(passcfg.CODE.SAFETY, code, MAX_BUFFER_SIZE - 1);
                break;
            default:
                break;
        }
    } else {
        switch (edit_field) {
            case 1:
                strncpy(passcfg.MESSAGE.BOOT, raw, MAX_BUFFER_SIZE - 1);
                break;
            case 3:
                strncpy(passcfg.MESSAGE.LAUNCH, raw, MAX_BUFFER_SIZE - 1);
                break;
            case 5:
                strncpy(passcfg.MESSAGE.SETTING, raw, MAX_BUFFER_SIZE - 1);
                break;
            default:
                break;
        }
    }

    save_passcode(&passcfg);
    update_value_labels();
    play_sound(SND_CONFIRM);
    toast_message(lang.MUXPASSCFG.SAVED, SHORT);
}

static void handle_keyboard_OK_press(void) {
    validate_and_save();

    int was_code = is_code_field(edit_field);
    key_show = 0;
    edit_field = -1;

    reset_osk(was_code ? hex_entry : key_entry);
    lv_textarea_set_text(ui_txtEntry_passcfg, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnlEntry_passcfg);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_KEYPRESS);

    lv_obj_t *active = (key_show == 3) ? hex_entry : key_entry;
    const char *is_key = lv_btnmatrix_get_btn_text(active, key_curr);

    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
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
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        key_backspace(ui_txtEntry_passcfg);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "passcfg");
    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txtEntry_passcfg);
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        key_show = 0;
        edit_field = -1;
        lv_textarea_set_text(ui_txtEntry_passcfg, "");
        lv_group_set_focus_cb(ui_group, NULL);
        osk_hide(ui_pnlEntry_passcfg);
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call) return;

    play_sound(SND_INFO_OPEN);
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

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, 0, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, passcfg, BootCode, lang.MUXPASSCFG.BOOTCODE, "boot_lock", code_display(passcfg.CODE.BOOT));
    INIT_VALUE_ITEM(-1, passcfg, BootMsg, lang.MUXPASSCFG.BOOTMSG, "boot_info", msg_display(passcfg.MESSAGE.BOOT));
    INIT_VALUE_ITEM(-1, passcfg, LaunchCode, lang.MUXPASSCFG.LAUNCHCODE, "launch_lock", code_display(passcfg.CODE.LAUNCH));
    INIT_VALUE_ITEM(-1, passcfg, LaunchMsg, lang.MUXPASSCFG.LAUNCHMSG, "launch_info", msg_display(passcfg.MESSAGE.LAUNCH));
    INIT_VALUE_ITEM(-1, passcfg, SettingCode, lang.MUXPASSCFG.SETTINGCODE, "setting_lock", code_display(passcfg.CODE.SETTING));
    INIT_VALUE_ITEM(-1, passcfg, SettingMsg, lang.MUXPASSCFG.SETTINGMSG, "setting_info", msg_display(passcfg.MESSAGE.SETTING));
    INIT_VALUE_ITEM(-1, passcfg, SafetyCode, lang.MUXPASSCFG.SAFETYCODE, "safety", code_display(passcfg.CODE.SAFETY));

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_OK_press();

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txtEntry_passcfg);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                0},
            {ui_lblNavA,      lang.GENERIC.EDIT, 0},
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

    overlay_display();
}

int muxpasscfg_main(void) {
    init_module(__func__);
    load_passcode(&passcfg);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPASSCFG.TITLE);
    init_muxpasscfg(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnlEntry_passcfg, ui_txtEntry_passcfg, 2, 0, 6);
    if (hex_entry && lv_obj_is_valid(hex_entry)) {
        lv_obj_add_flag(hex_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(hex_entry, LV_STATE_DISABLED);
    }

    init_timer(NULL, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_B] = handle_b_hold,
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_r1,
            }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
