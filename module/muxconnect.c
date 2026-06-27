#include "muxshare.h"
#include "ui/ui_muxconnect.h"

#define CONNECT(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CONNECT_ELEMENTS) };
#undef CONNECT

#define CONNECT(NAME, UDATA) static int NAME##_original;
CONNECT_ELEMENTS
#undef CONNECT

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define CONNECT(NAME, UDATA) {UDATA, lang.muxconnect.help.NAME},
        CONNECT_ELEMENTS
#undef CONNECT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int visible_network_opt(void) {
    return device.board.has_network;
}

static int visible_bluetooth_opt(void) {
    return device.board.has_bluetooth;
}

static void init_dropdown_settings(void) {
#define CONNECT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_connect);
    CONNECT_ELEMENTS
#undef CONNECT
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, connect, network, lang.muxconnect.network, "network", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, net_adv, lang.muxconnect.net_adv, "netadv", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, proxy, lang.muxconnect.proxy, "proxy", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, services, lang.muxconnect.services, "service", NULL, 0);
    INIT_OPTION_ITEM(-1, connect, bluetooth, lang.muxconnect.bluetooth, "bluetooth", NULL, 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 1);

    if (!visible_network_opt()) {
        HIDE_OPTION_ITEM(connect, network);
        HIDE_OPTION_ITEM(connect, net_adv);
        HIDE_OPTION_ITEM(connect, proxy);
        HIDE_OPTION_ITEM(connect, services);
    }

    if (!device.board.has_bluetooth) {
        HIDE_OPTION_ITEM(connect, bluetooth);
    }

    gen_step_movement(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1, 1, 0, 1);
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

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_general = 0,
        menu_option,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"net_profile", &kiosk.config.network, menu_general, visible_network_opt},
        {"netadv", &kiosk.config.net_settings, menu_general, visible_network_opt},
        {"net_proxy", &kiosk.config.proxy, menu_general, visible_network_opt},
        {"webserv", &kiosk.config.web_services, menu_general, visible_network_opt},
        {"btall", &kiosk_pass, menu_general, visible_bluetooth_opt}
    };

    const menu_entry *visible_entries[ui_count_dynamic];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    switch (entry->action) {
        case menu_general:
            if (is_ksk(*entry->kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(snd_confirm);
            load_mux(entry->mux_name);

            mux_input_stop();
            break;
        case menu_option:
            handle_option_next();
            break;
        default:
            break;
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "connect");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define CONNECT(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_connect, UDATA);
    CONNECT_ELEMENTS
#undef CONNECT

    overlay_display();
}

int muxconnect_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxconnect.title);
    init_muxconnect(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
