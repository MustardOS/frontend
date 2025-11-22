#include "muxshare.h"
#include "ui/ui_muxconnect.h"

#define UI_COUNT 5

#define CONNECT(NAME, UDATA) static int NAME##_original;
CONNECT_ELEMENTS
#undef CONNECT

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblNetwork_connect,     lang.MUXCONNECT.HELP.WIFI},
            {ui_lblNetAdv_connect,      lang.MUXCONNECT.HELP.NETADV},
            {ui_lblServices_connect,    lang.MUXCONNECT.HELP.WEB},
            {ui_lblBluetooth_connect,   lang.MUXCONNECT.HELP.BLUETOOTH},
            {ui_lblUsbFunction_connect, lang.MUXCONNECT.HELP.USB},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define CONNECT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_connect);
    CONNECT_ELEMENTS
#undef CONNECT
}

static void restore_options(void) {
    const char *usb_type = config.SETTINGS.ADVANCED.USBFUNCTION;
    if (strcasecmp(usb_type, "adb") == 0) {
        lv_dropdown_set_selected(ui_droUsbFunction_connect, 1);
    } else if (strcasecmp(usb_type, "mtp") == 0) {
        lv_dropdown_set_selected(ui_droUsbFunction_connect, 2);
    } else {
        lv_dropdown_set_selected(ui_droUsbFunction_connect, 0);
    }
    lv_dropdown_set_selected(ui_droBluetooth_connect, config.VISUAL.BLUETOOTH);
}

static void save_options(void) {
    char *idx_usbfunction;
    switch (lv_dropdown_get_selected(ui_droUsbFunction_connect)) {
        case 1:
            idx_usbfunction = "adb";
            break;
        case 2:
            idx_usbfunction = "mtp";
            break;
        default:
            idx_usbfunction = "none";
            break;
    }

    int is_modified = 0;

    int idx_bluetooth = lv_dropdown_get_selected(ui_droBluetooth_connect);
    if (lv_dropdown_get_selected(ui_droBluetooth_connect) != Bluetooth_original) {
        write_text_to_file(CONF_CONFIG_PATH "visual/bluetooth", "w", INT, idx_bluetooth);
    }

    if (lv_dropdown_get_selected(ui_droUsbFunction_connect) != UsbFunction_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/advanced/usb_function", "w", CHAR, idx_usbfunction);
    }

    if (is_modified > 0) run_tweak_script();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *usb_functions[] = {
            lang.GENERIC.DISABLED,
            lang.MUXCONNECT.ADB,
            lang.MUXCONNECT.MTP
    };

    INIT_OPTION_ITEM(-1, connect, Network, lang.MUXCONNECT.WIFI, "network", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, NetAdv, lang.MUXCONNECT.NETADV, "netadv", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, Services, lang.MUXCONNECT.WEB, "service", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, Bluetooth, lang.MUXCONNECT.BLUETOOTH, "bluetooth", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, UsbFunction, lang.MUXCONNECT.USB, "usbfunction", usb_functions, 3);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
        apply_text_long_dot(&theme, ui_objects_panel[i], ui_objects[i]);
    }

    if (!device.BOARD.HAS_NETWORK) {
        HIDE_OPTION_ITEM(connect, Network);
        HIDE_OPTION_ITEM(connect, NetAdv);
        HIDE_OPTION_ITEM(connect, Services);
    }

    if (!device.BOARD.HAS_BLUETOOTH || true) { // TODO: remove true when bluetooth is implemented
        HIDE_OPTION_ITEM(connect, Bluetooth);
    }

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;
    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"network", "network", &kiosk.CONFIG.NETWORK},
            {"netadv",  "netadv",  &kiosk.CONFIG.NET_SETTINGS},
            {"service", "webserv", &kiosk.CONFIG.WEB_SERVICES}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (is_ksk(*elements[i].kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);
            load_mux(elements[i].mux_name);

            close_input();
            mux_input_stop();

            break;
        }
    }

    handle_option_next();
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

    save_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "connect");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define CONNECT(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_connect, UDATA);
    CONNECT_ELEMENTS
#undef CONNECT

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxconnect_main(void) {
    init_module("muxconnect");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONNECT.TITLE);
    init_muxconnect(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
