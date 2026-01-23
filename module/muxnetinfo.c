#include "muxshare.h"
#include "ui/ui_muxnetinfo.h"

#define UI_COUNT 10

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblHostname_netinfo,  lang.MUXNETINFO.HELP.HOSTNAME},
            {ui_lblMac_netinfo,       lang.MUXNETINFO.HELP.MAC},
            {ui_lblIp_netinfo,        lang.MUXNETINFO.HELP.IP},
            {ui_lblSsid_netinfo,      lang.MUXNETINFO.HELP.SSID},
            {ui_lblGateway_netinfo,   lang.MUXNETINFO.HELP.GATEWAY},
            {ui_lblDns_netinfo,       lang.MUXNETINFO.HELP.DNS},
            {ui_lblSignal_netinfo,    lang.MUXNETINFO.HELP.SIGNAL},
            {ui_lblChannel_netinfo,   lang.MUXNETINFO.HELP.CHANNEL},
            {ui_lblAcTraffic_netinfo, lang.MUXNETINFO.HELP.ACTRAFFIC},
            {ui_lblTpTraffic_netinfo, lang.MUXNETINFO.HELP.TPTRAFFIC},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static const char *get_hostname(void) {
    char *result = read_line_char_from("/etc/hostname", 1);
    if (!result || result[0] == '\0') {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    static char hostname[64];
    snprintf(hostname, sizeof(hostname), "%s", result);
    free(result);

    return hostname;
}

static const char *get_mac_address(void) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", device.NETWORK.INTERFACE);

    if (!file_exist(path)) return lang.GENERIC.UNKNOWN;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat %s", path);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);

        char *big_mac = (CONF_CONFIG_PATH "network/mac");
        if (file_exist(big_mac)) return read_line_char_from(big_mac, 1);

        return lang.GENERIC.UNKNOWN;
    }

    static char mac[32];
    snprintf(mac, sizeof(mac), "%s", result);
    free(result);

    return mac;
}

static const char *get_ip_address(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip addr show %s | awk '/inet / {print $2}' | cut -d/ -f1", device.NETWORK.INTERFACE);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    static char ip[64];
    snprintf(ip, sizeof(ip), "%s", result);
    free(result);

    return ip;
}

static const char *get_ssid(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk -F': ' '/SSID/ {print $2}'", device.NETWORK.INTERFACE);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    static char ssid[64];
    snprintf(ssid, sizeof(ssid), "%s", result);
    free(result);

    return ssid;
}

static const char *get_gateway(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip route | awk '/default/ {print $3}'");

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    static char gw[64];
    snprintf(gw, sizeof(gw), "%s", result);
    free(result);

    return gw;
}

static const char *get_dns_servers(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "awk '/nameserver/ {print $2}' /etc/resolv.conf | xargs");

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    static char dns[128];
    snprintf(dns, sizeof(dns), "%s", result);
    free(result);

    return dns;
}

static const char *get_signal_strength(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/signal/ {print $2}'", device.NETWORK.INTERFACE);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    int dbm = safe_atoi(result);
    free(result);

    int index = dbm <= -100 ? 0 : (dbm >= 0 ? 100 : -dbm);

    // I think these values are correct?
    // https://www.intuitibits.com/2016/03/23/dbm-to-percent-conversion/
    static const uint8_t dbm_perc[101] = {
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                     // -100 to -91
            1, 3, 6, 10, 13, 17, 20, 22, 24, 26,              // -90 to -81
            28, 30, 32, 34, 36, 38, 40, 42, 44, 46,           // -80 to -71
            48, 50, 51, 53, 55, 56, 58, 60, 61, 63,           // -70 to -61
            64, 66, 67, 69, 70, 71, 73, 74, 75, 76,           // -60 to -51
            78, 79, 80, 81, 82, 83, 84, 85, 86, 87,           // -50 to -41
            88, 89, 90, 90, 91, 92, 93, 93, 94, 95,           // -40 to -31
            95, 96, 96, 97, 97, 98, 98, 99, 99, 100,          // -30 to -21
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, // -20 to -11
            100, 100, 100, 100, 100, 100, 100, 100, 100, 100, // -10 to -1
            100
    };

    int percent = dbm_perc[index];

    static char signal[32];
    snprintf(signal, sizeof(signal), "%d%% (%d dBm)", percent, dbm);

    return signal;
}

static const char *get_channel_info(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/freq:/ {print $2}'", device.NETWORK.INTERFACE);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.GENERIC.UNKNOWN;
    }

    int freq = safe_atoi(result);
    free(result);

    static const struct {
        int freq;
        int channel;
    } freq_table[] = {
            // 2.4 GHz
            {2412, 1},
            {2417, 2},
            {2422, 3},
            {2427, 4},
            {2432, 5},
            {2437, 6},
            {2442, 7},
            {2447, 8},
            {2452, 9},
            {2457, 10},
            {2462, 11},
            {2467, 12},
            {2472, 13},
            {2484, 14},

            // 3.65 GHz? No just kidding...?

            // 5 GHz
            {5180, 36},
            {5200, 40},
            {5220, 44},
            {5240, 48},
            {5260, 52},
            {5280, 56},
            {5300, 60},
            {5320, 64},
            {5500, 100},
            {5520, 104},
            {5540, 108},
            {5560, 112},
            {5580, 116},
            {5600, 120},
            {5620, 124},
            {5640, 128},
            {5660, 132},
            {5680, 136},
            {5700, 140},
            {5745, 149},
            {5765, 153},
            {5785, 157},
            {5805, 161},
            {5825, 165}

            // 6 GHz, maybe one day!
    };

    for (size_t i = 0; i < A_SIZE(freq_table); i++) {
        if (freq_table[i].freq == freq) {
            static char info[64];
            snprintf(info, sizeof(info), "%d MHz - %s %d", freq, lang.GENERIC.CHANNEL, freq_table[i].channel);
            return info;
        }
    }

    static char unknown[64];
    snprintf(unknown, sizeof(unknown), "%d MHz - %s %s", freq, lang.GENERIC.CHANNEL, lang.GENERIC.UNKNOWN);
    return unknown;
}

static const char *get_ac_traffic(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.NETWORK.INTERFACE);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.NETWORK.INTERFACE);

    unsigned long long rx = read_all_long_from(rx_path);
    unsigned long long tx = read_all_long_from(tx_path);

    static char ac_traffic[64];
    snprintf(ac_traffic, sizeof(ac_traffic), "RX: %.1f MB TX: %.1f MB",
             (double) rx / 1024.0 / 1024.0,
             (double) tx / 1024.0 / 1024.0);

    return ac_traffic;
}

static const char *get_tp_traffic(void) {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    static unsigned long long last_rx = 0, last_tx = 0;
    static time_t last_time = 0;
    double rx_rate = 0, tx_rate = 0;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.NETWORK.INTERFACE);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.NETWORK.INTERFACE);

    unsigned long long rx = read_all_long_from(rx_path);
    unsigned long long tx = read_all_long_from(tx_path);

    time_t now = time(NULL);
    if (last_time > 0) {
        double delta = difftime(now, last_time);
        if (delta > 0) {
            rx_rate = (double) (rx - last_rx) / delta;
            tx_rate = (double) (tx - last_tx) / delta;
        }
    }

    last_rx = rx;
    last_tx = tx;

    last_time = now;

    static char tp_traffic[64];
    snprintf(tp_traffic, sizeof(tp_traffic), "RX: %.1f KB/s TX: %.1f KB/s",
             rx_rate / 1024.0, tx_rate / 1024.0);

    return tp_traffic;
}

static void update_network_info() {
    lv_label_set_text(ui_lblSignalValue_netinfo, get_signal_strength());
    lv_label_set_text(ui_lblChannelValue_netinfo, get_channel_info());
    lv_label_set_text(ui_lblAcTrafficValue_netinfo, get_ac_traffic());
    lv_label_set_text(ui_lblTpTrafficValue_netinfo, get_tp_traffic());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, netinfo, Hostname, lang.MUXNETINFO.HOSTNAME, "hostname", get_hostname());
    INIT_VALUE_ITEM(-1, netinfo, Mac, lang.MUXNETINFO.MAC, "mac", get_mac_address());
    INIT_VALUE_ITEM(-1, netinfo, Ip, lang.MUXNETINFO.IP, "ip", get_ip_address());
    INIT_VALUE_ITEM(-1, netinfo, Ssid, lang.MUXNETINFO.SSID, "ssid", get_ssid());
    INIT_VALUE_ITEM(-1, netinfo, Gateway, lang.MUXNETINFO.GATEWAY, "gateway", get_gateway());
    INIT_VALUE_ITEM(-1, netinfo, Dns, lang.MUXNETINFO.DNS, "dns", get_dns_servers());
    INIT_VALUE_ITEM(-1, netinfo, Signal, lang.MUXNETINFO.SIGNAL, "signal", get_signal_strength());
    INIT_VALUE_ITEM(-1, netinfo, Channel, lang.MUXNETINFO.CHANNEL, "channel", get_channel_info());
    INIT_VALUE_ITEM(-1, netinfo, AcTraffic, lang.MUXNETINFO.ACTRAFFIC, "actraffic", get_ac_traffic());
    INIT_VALUE_ITEM(-1, netinfo, TpTraffic, lang.MUXNETINFO.TPTRAFFIC, "tptraffic", get_tp_traffic());

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!is_network_connected()) {
        HIDE_VALUE_ITEM(netinfo, Ip);
        HIDE_VALUE_ITEM(netinfo, Ssid);
        HIDE_VALUE_ITEM(netinfo, Gateway);
        HIDE_VALUE_ITEM(netinfo, Dns);
        HIDE_VALUE_ITEM(netinfo, Signal);
        HIDE_VALUE_ITEM(netinfo, Channel);
        HIDE_VALUE_ITEM(netinfo, AcTraffic);
        HIDE_VALUE_ITEM(netinfo, TpTraffic);
    }
}

static void check_focus() {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHostname_netinfo || element_focused == ui_lblMac_netinfo) {
        lv_label_set_text(ui_lblNavA, element_focused == ui_lblHostname_netinfo ? lang.GENERIC.EDIT : lang.GENERIC.CHANGE);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lblNavA, "");
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void reload_netinfo(void) {
    load_mux("netinfo");
    close_input();
    mux_input_stop();
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblHostname_netinfo) {
        const char *new_hostname = lv_textarea_get_text(ui_txtEntry_netinfo);
        if (strlen(new_hostname) < 3) goto clear_osk;

        play_sound(SND_CONFIRM);

        toast_message(lang.MUXNETINFO.SAVE.HOST, FOREVER);

        lv_label_set_text(ui_lblHostnameValue_netinfo, new_hostname);
        write_text_to_file("/etc/hostname", "w", CHAR, new_hostname);

        const char *hn_set_args[] = {"hostname", new_hostname, NULL};
        run_exec(hn_set_args, A_SIZE(hn_set_args), 0, 0, NULL, NULL);

        reload_netinfo();
    }

    clear_osk:
    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_netinfo, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else if (strcmp(is_key, OSK_UPPER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, OSK_CHAR) == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, OSK_LOWER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_back(void) {
    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netinfo");

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHostname_netinfo) {
        if (is_network_connected()) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETINFO.ERROR.EDIT, SHORT);
            return;
        }

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_netinfo);

        lv_textarea_set_text(ui_txtEntry_netinfo, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else if (element_focused == ui_lblMac_netinfo) {
        if (is_network_connected()) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETINFO.ERROR.CHANGE, SHORT);
            return;
        }

        play_sound(SND_CONFIRM);

        toast_message(lang.MUXNETINFO.SAVE.MAC, FOREVER);

        const char *mac_change_args[] = {OPT_PATH "script/web/macchange.sh", NULL};
        run_exec(mac_change_args, A_SIZE(mac_change_args), 0, 0, NULL, NULL);

        reload_netinfo();
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

    key_show ? close_osk(key_entry, ui_group, ui_txtEntry_netinfo, ui_pnlEntry_netinfo) : handle_back();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_backspace(ui_txtEntry_netinfo);
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_swap();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
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
    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (!key_show) handle_list_nav_page_down();
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                0},
            {ui_lblNavA,      lang.GENERIC.EDIT, 0},
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define NETINFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_netinfo, UDATA);
    NETINFO_ELEMENTS
#undef NETINFO

    overlay_display();
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_OK_press();
    }
    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txtEntry_netinfo);
    }
}

int muxnetinfo_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETINFO.TITLE);
    init_muxnetinfo(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnlEntry_netinfo, ui_txtEntry_netinfo, false);

    init_timer(ui_gen_refresh_task, update_network_info);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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
