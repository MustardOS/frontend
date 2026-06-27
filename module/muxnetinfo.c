#include "muxshare.h"
#include "ui/ui_muxnetinfo.h"

#define NETINFO(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(NETINFO_ELEMENTS) };
#undef NETINFO

static void list_nav_move(int steps, int direction);

static int interface_valid = 0;

static int is_valid_interface(const char *iface) {
    if (!iface || !*iface) return 0;
    for (const char *p = iface; *p; p++) {
        const char c = *p;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define NETINFO(NAME, UDATA) {UDATA, lang.muxnetinfo.help.NAME},
        NETINFO_ELEMENTS
#undef NETINFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static const char *get_hostname(void) {
    char *result = read_line_char_from("/etc/hostname", 1);
    if (!result || result[0] == '\0') {
        free(result);
        return lang.generic.unknown;
    }

    static char hostname[64];
    snprintf(hostname, sizeof(hostname), "%s", result);
    free(result);

    return hostname;
}

static const char *get_mac_address(void) {
    if (!interface_valid) return lang.generic.unknown;

    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", device.network.interface);

    FILE *f = fopen(path, "r");
    if (!f) {
        const char *big_mac = CONF_CONFIG_PATH "network/mac";
        if (file_exist(big_mac)) return read_line_char_from(big_mac, 1);

        return lang.generic.unknown;
    }

    static char mac[32];
    if (!fgets(mac, sizeof(mac), f)) {
        fclose(f);
        const char *big_mac = CONF_CONFIG_PATH "network/mac";
        if (file_exist(big_mac)) return read_line_char_from(big_mac, 1);

        return lang.generic.unknown;
    }

    fclose(f);

    const size_t len = strlen(mac);
    if (len > 0 && mac[len - 1] == '\n') mac[len - 1] = '\0';

    return mac;
}

static const char *get_ip_address(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip addr show %s | awk '/inet / {print $2}' | cut -d/ -f1", device.network.interface);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    static char ip[64];
    snprintf(ip, sizeof(ip), "%s", result);
    free(result);

    return ip;
}

static const char *get_ssid(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk -F': ' '/SSID/ {print $2}'", device.network.interface);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    static char ssid[64];
    snprintf(ssid, sizeof(ssid), "%s", result);
    free(result);

    return ssid;
}

static const char *get_gateway(void) {
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip route | awk '/default/ {print $3}'");

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    static char gw[64];
    snprintf(gw, sizeof(gw), "%s", result);
    free(result);

    return gw;
}

static const char *get_dns_servers(void) {
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "awk '/nameserver/ {print $2}' /etc/resolv.conf | xargs");

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    static char dns[128];
    snprintf(dns, sizeof(dns), "%s", result);
    free(result);

    return dns;
}

static const char *get_signal_strength(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/signal/ {print $2}'", device.network.interface);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    const int dbm = safe_atoi(result, 0);
    free(result);

    const int index = dbm <= -100 ? 0 : dbm >= 0 ? 100 : -dbm;

    // I think these values are correct?
    // https://www.intuitibits.com/2016/03/23/dbm-to-percent-conversion/
    static const uint8_t dbm_perc[101] = {1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   // -100 to -91
                                          1,   3,   6,   10,  13,  17,  20,  22,  24,  26,  // -90 to -81
                                          28,  30,  32,  34,  36,  38,  40,  42,  44,  46,  // -80 to -71
                                          48,  50,  51,  53,  55,  56,  58,  60,  61,  63,  // -70 to -61
                                          64,  66,  67,  69,  70,  71,  73,  74,  75,  76,  // -60 to -51
                                          78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  // -50 to -41
                                          88,  89,  90,  90,  91,  92,  93,  93,  94,  95,  // -40 to -31
                                          95,  96,  96,  97,  97,  98,  98,  99,  99,  100, // -30 to -21
                                          100, 100, 100, 100, 100, 100, 100, 100, 100, 100, // -20 to -11
                                          100, 100, 100, 100, 100, 100, 100, 100, 100, 100, // -10 to -1
                                          100};

    const int percent = dbm_perc[index];

    static char signal[32];
    snprintf(signal, sizeof(signal), "%d%% (%d dBm)", percent, dbm);

    return signal;
}

static const char *get_channel_info(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/freq:/ {print $2}'", device.network.interface);

    char *result = get_execute_result(cmd, 0);
    if (!result || !*result) {
        free(result);
        return lang.generic.unknown;
    }

    const int freq = safe_atoi(result, 0);
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
            snprintf(info, sizeof(info), "%d MHz - %s %d", freq, lang.generic.channel, freq_table[i].channel);
            return info;
        }
    }

    static char unknown[64];
    snprintf(unknown, sizeof(unknown), "%d MHz - %s %s", freq, lang.generic.channel, lang.generic.unknown);
    return unknown;
}

static const char *get_ac_traffic(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.network.interface);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.network.interface);

    const unsigned long long rx = read_all_long_from(rx_path);
    const unsigned long long tx = read_all_long_from(tx_path);

    static char ac_traffic[64];
    snprintf(
        ac_traffic, sizeof(ac_traffic), "RX: %.1f MB TX: %.1f MB", (double) rx / 1024.0 / 1024.0,
        (double) tx / 1024.0 / 1024.0
    );

    return ac_traffic;
}

static const char *get_tp_traffic(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    static unsigned long long last_rx = 0, last_tx = 0;
    static time_t last_time = 0;
    double rx_rate = 0, tx_rate = 0;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.network.interface);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.network.interface);

    const unsigned long long rx = read_all_long_from(rx_path);
    const unsigned long long tx = read_all_long_from(tx_path);

    const time_t now = time(NULL);
    if (last_time > 0) {
        const double delta = difftime(now, last_time);
        if (delta > 0) {
            rx_rate = (double) (rx - last_rx) / delta;
            tx_rate = (double) (tx - last_tx) / delta;
        }
    }

    last_rx = rx;
    last_tx = tx;

    last_time = now;

    static char tp_traffic[64];
    snprintf(tp_traffic, sizeof(tp_traffic), "RX: %.1f KB/s TX: %.1f KB/s", rx_rate / 1024.0, tx_rate / 1024.0);

    return tp_traffic;
}

static void export_diagnostics(void) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/netdiag.txt", device.storage.rom.mount);

    FILE *f = fopen(path, "w");
    if (!f) {
        toast_message(lang.muxnetinfo.report.fail, tst_wait_m);
        return;
    }

    fprintf(f, "%s: %s\n", lang.muxnetinfo.hostname, get_hostname());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.mac, get_mac_address());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.ip, get_ip_address());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.ssid, get_ssid());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.gateway, get_gateway());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.dns, get_dns_servers());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.signal, get_signal_strength());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.channel, get_channel_info());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.actraffic, get_ac_traffic());
    fprintf(f, "%s: %s\n", lang.muxnetinfo.tptraffic, get_tp_traffic());

    fclose(f);
    toast_message(lang.muxnetinfo.report.success, tst_wait_m);
}

static void update_network_info(const lv_timer_t *timer) {
    (void) timer;
    lv_label_set_text(ui_val_signal_netinfo, get_signal_strength());
    lv_label_set_text(ui_val_channel_netinfo, get_channel_info());
    lv_label_set_text(ui_val_ac_traffic_netinfo, get_ac_traffic());
    lv_label_set_text(ui_val_tp_traffic_netinfo, get_tp_traffic());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, netinfo, hostname, lang.muxnetinfo.hostname, "hostname", get_hostname());
    INIT_VALUE_ITEM(-1, netinfo, mac, lang.muxnetinfo.mac, "mac", get_mac_address());
    INIT_VALUE_ITEM(-1, netinfo, ip, lang.muxnetinfo.ip, "ip", get_ip_address());
    INIT_VALUE_ITEM(-1, netinfo, ssid, lang.muxnetinfo.ssid, "ssid", get_ssid());
    INIT_VALUE_ITEM(-1, netinfo, gateway, lang.muxnetinfo.gateway, "gateway", get_gateway());
    INIT_VALUE_ITEM(-1, netinfo, dns, lang.muxnetinfo.dns, "dns", get_dns_servers());
    INIT_VALUE_ITEM(-1, netinfo, signal, lang.muxnetinfo.signal, "signal", get_signal_strength());
    INIT_VALUE_ITEM(-1, netinfo, channel, lang.muxnetinfo.channel, "channel", get_channel_info());
    INIT_VALUE_ITEM(-1, netinfo, ac_traffic, lang.muxnetinfo.actraffic, "actraffic", get_ac_traffic());
    INIT_VALUE_ITEM(-1, netinfo, tp_traffic, lang.muxnetinfo.tptraffic, "tptraffic", get_tp_traffic());

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!is_network_connected()) {
        HIDE_VALUE_ITEM(netinfo, ip);
        HIDE_VALUE_ITEM(netinfo, ssid);
        HIDE_VALUE_ITEM(netinfo, gateway);
        HIDE_VALUE_ITEM(netinfo, dns);
        HIDE_VALUE_ITEM(netinfo, signal);
        HIDE_VALUE_ITEM(netinfo, channel);
        HIDE_VALUE_ITEM(netinfo, ac_traffic);
        HIDE_VALUE_ITEM(netinfo, tp_traffic);
    }
}

static void check_focus() {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_hostname_netinfo || e_focused == ui_lbl_mac_netinfo) {
        lv_label_set_text(ui_lbl_nav_a, e_focused == ui_lbl_hostname_netinfo ? lang.generic.edit : lang.generic.change);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lbl_nav_a, "");
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void reload_netinfo(void) {
    load_mux("netinfo");
    mux_input_stop();
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_hostname_netinfo) {
        const char *new_hostname = lv_textarea_get_text(ui_txt_entry_netinfo);
        if (strlen(new_hostname) < 3) goto clear_osk;

        play_sound(snd_confirm);

        toast_message(lang.muxnetinfo.save.host, tst_wait_f);

        lv_label_set_text(ui_val_hostname_netinfo, new_hostname);
        write_text_to_file("/etc/hostname", "w", CHAR, new_hostname);

        const char *hn_set_args[] = {"hostname", new_hostname, NULL};
        run_exec(hn_set_args, A_SIZE(hn_set_args), 0, 0, NULL, NULL);

        reload_netinfo();
    }

clear_osk:
    reset_osk(key_entry);

    lv_textarea_set_text(ui_txt_entry_netinfo, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnl_entry_netinfo);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_back(void) {
    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netinfo");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_hostname_netinfo) {
        if (is_network_connected()) {
            play_sound(snd_error);
            toast_message(lang.muxnetinfo.error.edit, tst_wait_s);
            return;
        }

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;
        osk_show(ui_pnl_entry_netinfo);
        lv_textarea_set_text(ui_txt_entry_netinfo, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else if (e_focused == ui_lbl_mac_netinfo) {
        if (is_network_connected()) {
            play_sound(snd_error);
            toast_message(lang.muxnetinfo.error.change, tst_wait_s);
            return;
        }

        play_sound(snd_confirm);

        toast_message(lang.muxnetinfo.save.mac, tst_wait_f);

        const char *mac_change_args[] = {OPT_PATH "script/web/macchange.sh", NULL};
        run_exec(mac_change_args, A_SIZE(mac_change_args), 0, 0, NULL, NULL);

        reload_netinfo();
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_netinfo);
        return;
    }

    handle_back();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_netinfo);
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txt_entry_netinfo, ui_pnl_entry_netinfo);
        return;
    }

    play_sound(snd_confirm);
    export_diagnostics();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_space(ui_txt_entry_netinfo);
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
    if (key_show) {
        key_swap_back();
        return;
    }

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    handle_list_nav_page_down();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.edit, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.output, 0},
                                  {NULL, NULL, 0}});

#define NETINFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_netinfo, UDATA);
    NETINFO_ELEMENTS
#undef NETINFO

    overlay_display();
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_ok_press();
    }
    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txt_entry_netinfo);
    }
}

int muxnetinfo_main(void) {
    init_module(__func__);

    interface_valid = is_valid_interface(device.network.interface);
    if (!interface_valid) LOG_ERROR(mux_module, "Invalid network interface name: '%s'", device.network.interface);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxnetinfo.title);
    init_muxnetinfo(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_osk(ui_pnl_entry_netinfo, ui_txt_entry_netinfo, 0, 0, OSK_MAX);

    init_timer(ui_gen_refresh_task, update_network_info);
    list_nav_next(0);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
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

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
