#include "muxshare.h"
#include "ui/ui_muxbtall.h"

#define BTALL(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(BTALL_ELEMENTS)
};
#undef BTALL

#define BTALL(NAME, ENUM, UDATA) static int NAME##_original;
BTALL_ELEMENTS
#undef BTALL

static void show_help(void) {
    struct help_msg help_messages[] = {
#define BTALL(NAME, ENUM, UDATA) { UDATA, lang.MUXBTALL.HELP.ENUM },
            BTALL_ELEMENTS
#undef BTALL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define BTALL(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_btall);
    BTALL_ELEMENTS
#undef BTALL
}

static void restore_btall_options(void) {
    lv_dropdown_set_selected(ui_droAutoConnect_btall, config.BLUETOOTH.AUTOCONNECT);
}

static void save_btall_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(btall, AutoConnect, "bluetooth/autoconnect", INT, 0);

    if (is_modified > 0) {
        const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "autoconnect", NULL};
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

        char *tok = line;
        char *sp = strchr(tok, ' ');
        if (!sp || sp - tok > 17) continue;
        memcpy(mac, tok, sp - tok);
        mac[sp - tok] = '\0';

        tok = sp + 1;
        char *end;
        long val = strtol(tok, &end, 10);
        if (end == tok || *end != ' ' || val < 0 || val > 1) continue;
        connected = (int) val;

        snprintf(name, sizeof(name), "%s", end + 1);
        if (name[0] == '\0') continue;

        ui_count++;

        lv_obj_t *ui_pnlDevice = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlDevice);

        lv_obj_t *ui_lblDevice = lv_label_create(ui_pnlDevice);
        apply_theme_list_item(&theme, ui_lblDevice, name);

        lv_obj_t *ui_lblDeviceStatus = lv_label_create(ui_pnlDevice);
        apply_theme_list_value(&theme, ui_lblDeviceStatus, connected ? lang.MUXBTALL.CONNECTED : lang.MUXBTALL.DISCONNECTED);

        lv_obj_t *ui_icoDevice = lv_img_create(ui_pnlDevice);
        apply_theme_list_glyph(&theme, ui_icoDevice, mux_module, "bluetooth");

        lv_group_add_obj(ui_group, ui_lblDevice);
        lv_group_add_obj(ui_group_value, ui_lblDeviceStatus);
        lv_group_add_obj(ui_group_glyph, ui_icoDevice);
        lv_group_add_obj(ui_group_panel, ui_pnlDevice);

        lv_obj_set_user_data(ui_pnlDevice, strdup(mac));

        apply_size_to_content(&theme, ui_pnlContent, ui_lblDevice, ui_icoDevice, name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblDevice);
    }
    fclose(file);

    if (ui_count > UI_COUNT) lv_obj_update_layout(ui_pnlContent);
}

static void bt_poll_task(lv_timer_t *t) {
    if (!bt_list_pending) {
        lv_timer_del(t);
        bt_poll_timer = NULL;
        return;
    }

    struct stat st;
    int file_ready = (stat(CONF_CONFIG_PATH "bluetooth/paired", &st) == 0);
    int timed_out = (time(NULL) - bt_list_start >= 10);

    if (!file_ready && !timed_out) return;

    bt_list_pending = 0;
    lv_timer_del(t);
    bt_poll_timer = NULL;

    populate_paired_device_list();

    lv_label_set_text(ui_lblScreenMessage, ui_count <= UI_COUNT ? lang.MUXBTALL.NONE : "");

    if (ui_count > UI_COUNT) {
        nav_silent = 1;
        list_nav_next(0);
        nav_silent = 0;
        check_focus();
    }
}

static void create_paired_device_items(void) {
    lv_label_set_text(ui_lblScreenMessage, lang.MUXBTALL.LOADING);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);

    remove(CONF_CONFIG_PATH "bluetooth/paired");

    bt_list_start = time(NULL);
    bt_list_pending = 1;

    const char *args[] = {(OPT_PATH "script/mux/bt_device.sh"), "list", NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);
}

static const char *get_focused_device_mac(void) {
    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return NULL;

    return (const char *) lv_obj_get_user_data(panel);
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
    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lblAutoConnect_btall) {
        nav_show_a(0, NULL);
        nav_show_lr(1);
    } else if (e_focused) {
        nav_show_a(1, lang.GENERIC.SELECT);
        nav_show_lr(0);
    } else {
        nav_show_a(0, NULL);
        nav_show_lr(0);
    }
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active || current_item_index >= UI_COUNT) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || current_item_index >= UI_COUNT) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    static int16_t KIOSK_PASS = 0;

    typedef enum {
        MENU_GENERAL = 0,
        MENU_OPTION,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {NULL, &KIOSK_PASS, MENU_OPTION},  // Auto Connect
    };

    if (current_item_index < UI_COUNT) {
        const menu_entry *entry = &entries[current_item_index];
        if (entry->action == MENU_GENERAL) {
            play_sound(SND_CONFIRM);
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

    play_sound(SND_CONFIRM);
    cancel_bt_poll();

    char mac_copy[18];
    snprintf(mac_copy, sizeof(mac_copy), "%s", mac);

    const char *info_args[] = {(OPT_PATH "script/mux/bt_device.sh"), "info", mac_copy, NULL};
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

    play_sound(SND_CONFIRM);
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

    play_sound(SND_BACK);
    cancel_bt_poll();
    save_btall_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "bluetooth");
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    if (current_item_index >= UI_COUNT) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *auto_connect_options[] = {
            lang.GENERIC.DISABLED,
            lang.GENERIC.ENABLED
    };

    INIT_OPTION_ITEM(-1, btall, AutoConnect, lang.MUXBTALL.AUTOCONNECT, "autoconnect", auto_connect_options, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, true);

    create_paired_device_items();
    list_nav_next(0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.SCAN,   0},
            {NULL, NULL,                            0}
    });

    check_focus();

#define BTALL(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_btall, UDATA);
    BTALL_ELEMENTS
#undef BTALL

    overlay_display();
}

int muxbtall_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXBTALL.TITLE);
    init_muxbtall(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_btall_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    bt_poll_timer = lv_timer_create(bt_poll_task, 300, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);

    mux_input_task(&input_opts);

    return 0;
}
