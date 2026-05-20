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

#define BTDEV_STATUS_IDX BTDEV_INFO_COUNT
#define BTDEV_FORGET_IDX (BTDEV_INFO_COUNT + 1)

static char selected_mac[18];
static int status_original;

static void show_help(void) {
    struct help_msg help_messages[] = {
#define BTDEV(NAME, ENUM, UDATA) { UDATA, lang.MUXBTDEV.HELP.ENUM },
            BTDEV_ELEMENTS
#undef BTDEV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
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

static void check_focus(void) {
    if (current_item_index < BTDEV_STATUS_IDX) {
        nav_show_a(0, NULL);
        nav_show_lr(0);
    } else if (current_item_index == BTDEV_STATUS_IDX) {
        nav_show_a(0, NULL);
        nav_show_lr(1);
    } else {
        nav_show_a(1, lang.MUXBTDEV.FORGET);
        nav_show_lr(0);
    }
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
    if (msgbox_active || current_item_index != BTDEV_STATUS_IDX) return;
    move_option(ui_droStatus_btdev, -1);
}

static void handle_option_next(void) {
    if (msgbox_active || current_item_index != BTDEV_STATUS_IDX) return;
    move_option(ui_droStatus_btdev, +1);
}

static void save_btdev_options(void) {
    int current_status = lv_dropdown_get_selected(ui_droStatus_btdev);
    if (current_status == status_original || !*selected_mac) return;

    if (current_status == 1) {
        const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "connect", selected_mac, NULL};
        run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
    } else {
        const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "disconnect", selected_mac, NULL};
        run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
    }
}

static void handle_a(void) {
    if (hold_call) return;

    if (msgbox_active) {
        if (*selected_mac) {
            const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "forget", selected_mac, NULL};
            run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
        }
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        load_mux("btall");
        mux_input_stop();
        return;
    }

    if (current_item_index < BTDEV_STATUS_IDX) return;

    if (current_item_index == BTDEV_STATUS_IDX) {
        handle_option_next();
        return;
    }

    play_sound(SND_INFO_OPEN);
    show_info_box(lang.MUXBTDEV.FORGET, lang.MUXBTDEV.FORGET_CONFIRM, 0);
    msgbox_active = 1;
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

    play_sound(SND_BACK);
    save_btdev_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "btall");
    mux_input_stop();
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
    char val_type[MAX_BUFFER_SIZE];
    char val_battery[MAX_BUFFER_SIZE];
    char val_signal[MAX_BUFFER_SIZE];
    char val_conn[MAX_BUFFER_SIZE];

    read_device_info_field("Name", val_name);
    read_device_info_field("Type", val_type);
    read_device_info_field("Battery", val_battery);
    read_device_info_field("Signal", val_signal);
    read_device_info_field("Connected", val_conn);

    int connected = (strcmp(val_conn, "yes") == 0);

    const char *name = *val_name ? val_name : lang.GENERIC.UNKNOWN;
    const char *type = *val_type ? val_type : lang.GENERIC.UNKNOWN;
    const char *battery = *val_battery ? val_battery : lang.GENERIC.UNKNOWN;
    const char *signal = *val_signal ? val_signal : lang.GENERIC.UNKNOWN;

    INIT_VALUE_ITEM(-1, btdev, FriendlyName, lang.MUXBTDEV.FRIENDLYNAME, "friendlyname", name);
    INIT_VALUE_ITEM(-1, btdev, Type, lang.MUXBTDEV.TYPE, "type", type);
    INIT_VALUE_ITEM(-1, btdev, Battery, lang.MUXBTDEV.BATTERY, "battery", battery);
    INIT_VALUE_ITEM(-1, btdev, Signal, lang.MUXBTDEV.SIGNAL, "signal", signal);

    char *status_options[] = {
            lang.MUXBTDEV.DISCONNECTED,
            lang.MUXBTDEV.CONNECTED,
    };

    INIT_OPTION_ITEM(-1, btdev, Status, lang.MUXBTDEV.STATUS, "status", status_options, 2);
    lv_dropdown_set_selected(ui_droStatus_btdev, connected ? 1 : 0);

    INIT_OPTION_ITEM(-1, btdev, Forget, lang.MUXBTDEV.FORGET, "forget", NULL, 0);

    status_original = lv_dropdown_get_selected(ui_droStatus_btdev);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                   0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE,  0},
            {ui_lblNavAGlyph,  "",                   0},
            {ui_lblNavA,       lang.MUXBTDEV.FORGET, 0},
            {ui_lblNavBGlyph,  "",                   0},
            {ui_lblNavB,       lang.GENERIC.BACK,    0},
            {NULL, NULL,                             0}
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
        fgets(selected_mac, sizeof(selected_mac), f);
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
    init_muxbtdev(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_list_nav_down,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2]   = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN]  = handle_list_nav_down_hold,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_L2]         = hold_call_set,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
