#include "muxshare.h"
#include "ui/ui_muxconnect.h"

#define UI_COUNT 5

#define CONNECT(NAME, ENUM, UDATA) static int NAME##_original;
CONNECT_ELEMENTS
#undef CONNECT

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    struct help_msg help_messages[] = {
#define CONNECT(NAME, ENUM, UDATA) { lang.MUXCONNECT.HELP.ENUM },
            CONNECT_ELEMENTS
#undef CONNECT
    };

    gen_help(current_item_index, UI_COUNT, help_messages, ui_group, items);
}

static int visible_network_opt(void) {
    return device.BOARD.HASNETWORK;
}

static void init_dropdown_settings(void) {
#define CONNECT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_connect);
    CONNECT_ELEMENTS
#undef CONNECT
}

static void restore_options(void) {
    lv_dropdown_set_selected(ui_droUsbFunction_connect, config.SETTINGS.ADVANCED.USBFUNCTION);
    lv_dropdown_set_selected(ui_droBluetooth_connect, config.VISUAL.BLUETOOTH);
}

static void save_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(connect, UsbFunction, "settings/advanced/usb_function", INT, 0);
    CHECK_AND_SAVE_STD(connect, Bluetooth, "visual/bluetooth", INT, 0);

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

    INIT_OPTION_ITEM(-1, connect, Network, lang.MUXCONNECT.NETWORK, "network", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, NetAdv, lang.MUXCONNECT.NETADV, "netadv", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, Services, lang.MUXCONNECT.SERVICES, "service", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, Bluetooth, lang.MUXCONNECT.BLUETOOTH, "bluetooth", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, UsbFunction, lang.MUXCONNECT.USBFUNCTION, "usbfunction", usb_functions, 3);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, true);

    if (!visible_network_opt()) {
        HIDE_OPTION_ITEM(connect, Network);
        HIDE_OPTION_ITEM(connect, NetAdv);
        HIDE_OPTION_ITEM(connect, Services);
    }

    if (!device.BOARD.HASBLUETOOTH || true) { // TODO: remove true when bluetooth is implemented
        HIDE_OPTION_ITEM(connect, Bluetooth);
    }

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    static int16_t KIOSK_PASS = 0;

    typedef enum {
        MENU_GENERAL = 0,
        MENU_OPTION,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"network", &kiosk.CONFIG.NETWORK,      MENU_GENERAL, visible_network_opt},
            {"netadv",  &kiosk.CONFIG.NET_SETTINGS, MENU_GENERAL, visible_network_opt},
            {"webserv", &kiosk.CONFIG.WEB_SERVICES, MENU_GENERAL, visible_network_opt},
            {NULL,      &KIOSK_PASS,                MENU_OPTION, NULL}, // USB Function
    };

    const menu_entry *visible_entries[UI_COUNT];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    switch (entry->action) {
        case MENU_GENERAL:
            if (is_ksk(*entry->kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);
            load_mux(entry->mux_name);

            close_input();
            mux_input_stop();
            break;
        case MENU_OPTION:
            handle_option_next();
            break;
        default:
            return;
    }
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
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define CONNECT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_connect, UDATA);
    CONNECT_ELEMENTS
#undef CONNECT

    overlay_display();
}

int muxconnect_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONNECT.TITLE);
    init_muxconnect(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);

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
