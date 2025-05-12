#include "muxshare.h"
#include "muxnetinfo.h"
#include "ui/ui_muxnetinfo.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/input/list_nav.h"

#define UI_COUNT 10
static lv_obj_t *ui_objects[UI_COUNT];
#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

const char *hostname_original;

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblHostname_netinfo,  lang.MUXNETINFO.HELP.HOSTNAME},
            {ui_lblMAC_netinfo,       lang.MUXNETINFO.HELP.MAC},
            {ui_lblIP_netinfo,        lang.MUXNETINFO.HELP.IP},
            {ui_lblSSID_netinfo,      lang.MUXNETINFO.HELP.SSID},
            {ui_lblGateway_netinfo,   lang.MUXNETINFO.HELP.GATEWAY},
            {ui_lblDNS_netinfo,       lang.MUXNETINFO.HELP.DNS},
            {ui_lblSignal_netinfo,    lang.MUXNETINFO.HELP.SIGNAL},
            {ui_lblChannel_netinfo,   lang.MUXNETINFO.HELP.CHANNEL},
            {ui_lblACTraffic_netinfo, lang.MUXNETINFO.HELP.ACTRAFFIC},
            {ui_lblTPTraffic_netinfo, lang.MUXNETINFO.HELP.TPTRAFFIC},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static const char *get_hostname() {
    const char *result = read_line_from_file("/etc/hostname", 1);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char hostname[64];
    snprintf(hostname, sizeof(hostname), "%s", result);

    return hostname;
}

static const char *get_mac_address() {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", device.NETWORK.INTERFACE);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat %s", path);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char mac[32];
    snprintf(mac, sizeof(mac), "%s", result);

    return mac;
}

static const char *get_ip_address() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip addr show %s | awk '/inet / {print $2}' | cut -d/ -f1", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char ip[64];
    snprintf(ip, sizeof(ip), "%s", result);

    return ip;
}

static const char *get_ssid() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk -F': ' '/SSID/ {print $2}'", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char ssid[64];
    snprintf(ssid, sizeof(ssid), "%s", result);

    return ssid;
}

static const char *get_gateway() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    const char *result = get_execute_result("ip route | awk '/default/ {print $3}'");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char gw[64];
    snprintf(gw, sizeof(gw), "%s", result);

    return gw;
}

static const char *get_dns_servers() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    const char *result = get_execute_result("awk '/nameserver/ {print $2}' /etc/resolv.conf | xargs");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char dns[128];
    snprintf(dns, sizeof(dns), "%s", result);

    return dns;
}

static const char *get_signal_strength() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

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

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/signal/ {print $2}'", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    int dbm = safe_atoi(result);
    int index = dbm <= -100 ? 0 : (dbm >= 0 ? 100 : -dbm);
    int percent = dbm_perc[index];

    static char signal[32];
    snprintf(signal, sizeof(signal), "%d%% (%d dBm)", percent, dbm);

    return signal;
}

static const char *get_channel_info() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk '/freq:/ {print $2}'", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    int freq = safe_atoi(result);

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

    for (size_t i = 0; i < sizeof(freq_table) / sizeof(freq_table[0]); i++) {
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

static const char *get_ac_traffic() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.NETWORK.INTERFACE);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.NETWORK.INTERFACE);

    unsigned long long rx = read_ll_from_file(rx_path);
    unsigned long long tx = read_ll_from_file(tx_path);

    static char ac_traffic[64];
    snprintf(ac_traffic, sizeof(ac_traffic), "RX: %.1f MB TX: %.1f MB",
             rx / 1024.0 / 1024.0, tx / 1024.0 / 1024.0);

    return ac_traffic;
}

static const char *get_tp_traffic() {
    if (!is_network_connected()) return lang.GENERIC.NOT_CONNECTED;

    static unsigned long long last_rx = 0, last_tx = 0;
    static time_t last_time = 0;
    double rx_rate = 0, tx_rate = 0;

    char rx_path[128], tx_path[128];
    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", device.NETWORK.INTERFACE);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", device.NETWORK.INTERFACE);

    unsigned long long rx = read_ll_from_file(rx_path);
    unsigned long long tx = read_ll_from_file(tx_path);

    time_t now = time(NULL);
    if (last_time > 0) {
        double delta = difftime(now, last_time);
        if (delta > 0) {
            rx_rate = (rx - last_rx) / delta;
            tx_rate = (tx - last_tx) / delta;
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
    lv_label_set_text(ui_lblHostnameValue_netinfo, get_hostname());
    lv_label_set_text(ui_lblMACValue_netinfo, get_mac_address());
    lv_label_set_text(ui_lblIPValue_netinfo, get_ip_address());
    lv_label_set_text(ui_lblSSIDValue_netinfo, get_ssid());
    lv_label_set_text(ui_lblGatewayValue_netinfo, get_gateway());
    lv_label_set_text(ui_lblDNSValue_netinfo, get_dns_servers());
    lv_label_set_text(ui_lblSignalValue_netinfo, get_signal_strength());
    lv_label_set_text(ui_lblChannelValue_netinfo, get_channel_info());
    lv_label_set_text(ui_lblACTrafficValue_netinfo, get_ac_traffic());
    lv_label_set_text(ui_lblTPTrafficValue_netinfo, get_tp_traffic());
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlHostname_netinfo,
            ui_pnlMAC_netinfo,
            ui_pnlIP_netinfo,
            ui_pnlSSID_netinfo,
            ui_pnlGateway_netinfo,
            ui_pnlDNS_netinfo,
            ui_pnlSignal_netinfo,
            ui_pnlChannel_netinfo,
            ui_pnlACTraffic_netinfo,
            ui_pnlTPTraffic_netinfo
    };

    ui_objects[0] = ui_lblHostname_netinfo;
    ui_objects[1] = ui_lblMAC_netinfo;
    ui_objects[2] = ui_lblIP_netinfo;
    ui_objects[3] = ui_lblSSID_netinfo;
    ui_objects[4] = ui_lblGateway_netinfo;
    ui_objects[5] = ui_lblDNS_netinfo;
    ui_objects[6] = ui_lblSignal_netinfo;
    ui_objects[7] = ui_lblChannel_netinfo;
    ui_objects[8] = ui_lblACTraffic_netinfo;
    ui_objects[9] = ui_lblTPTraffic_netinfo;

    lv_obj_t *ui_objects_value[] = {
            ui_lblHostnameValue_netinfo,
            ui_lblMACValue_netinfo,
            ui_lblIPValue_netinfo,
            ui_lblSSIDValue_netinfo,
            ui_lblGatewayValue_netinfo,
            ui_lblDNSValue_netinfo,
            ui_lblSignalValue_netinfo,
            ui_lblChannelValue_netinfo,
            ui_lblACTrafficValue_netinfo,
            ui_lblTPTrafficValue_netinfo
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoHostname_netinfo,
            ui_icoMAC_netinfo,
            ui_icoIP_netinfo,
            ui_icoSSID_netinfo,
            ui_icoGateway_netinfo,
            ui_icoDNS_netinfo,
            ui_icoSignal_netinfo,
            ui_icoChannel_netinfo,
            ui_icoACTraffic_netinfo,
            ui_icoTPTraffic_netinfo
    };

    apply_theme_list_panel(ui_pnlHostname_netinfo);
    apply_theme_list_panel(ui_pnlMAC_netinfo);
    apply_theme_list_panel(ui_pnlIP_netinfo);
    apply_theme_list_panel(ui_pnlSSID_netinfo);
    apply_theme_list_panel(ui_pnlGateway_netinfo);
    apply_theme_list_panel(ui_pnlDNS_netinfo);
    apply_theme_list_panel(ui_pnlSignal_netinfo);
    apply_theme_list_panel(ui_pnlChannel_netinfo);
    apply_theme_list_panel(ui_pnlACTraffic_netinfo);
    apply_theme_list_panel(ui_pnlTPTraffic_netinfo);

    apply_theme_list_item(&theme, ui_lblHostname_netinfo, lang.MUXNETINFO.HOSTNAME);
    apply_theme_list_item(&theme, ui_lblMAC_netinfo, lang.MUXNETINFO.MAC);
    apply_theme_list_item(&theme, ui_lblIP_netinfo, lang.MUXNETINFO.IP);
    apply_theme_list_item(&theme, ui_lblSSID_netinfo, lang.MUXNETINFO.SSID);
    apply_theme_list_item(&theme, ui_lblGateway_netinfo, lang.MUXNETINFO.GATEWAY);
    apply_theme_list_item(&theme, ui_lblDNS_netinfo, lang.MUXNETINFO.DNS);
    apply_theme_list_item(&theme, ui_lblSignal_netinfo, lang.MUXNETINFO.SIGNAL);
    apply_theme_list_item(&theme, ui_lblChannel_netinfo, lang.MUXNETINFO.CHANNEL);
    apply_theme_list_item(&theme, ui_lblACTraffic_netinfo, lang.MUXNETINFO.ACTRAFFIC);
    apply_theme_list_item(&theme, ui_lblTPTraffic_netinfo, lang.MUXNETINFO.TPTRAFFIC);

    apply_theme_list_glyph(&theme, ui_icoHostname_netinfo, mux_module, "hostname");
    apply_theme_list_glyph(&theme, ui_icoMAC_netinfo, mux_module, "mac");
    apply_theme_list_glyph(&theme, ui_icoIP_netinfo, mux_module, "ip");
    apply_theme_list_glyph(&theme, ui_icoSSID_netinfo, mux_module, "ssid");
    apply_theme_list_glyph(&theme, ui_icoGateway_netinfo, mux_module, "gateway");
    apply_theme_list_glyph(&theme, ui_icoDNS_netinfo, mux_module, "dns");
    apply_theme_list_glyph(&theme, ui_icoSignal_netinfo, mux_module, "signal");
    apply_theme_list_glyph(&theme, ui_icoChannel_netinfo, mux_module, "channel");
    apply_theme_list_glyph(&theme, ui_icoACTraffic_netinfo, mux_module, "actraffic");
    apply_theme_list_glyph(&theme, ui_icoTPTraffic_netinfo, mux_module, "tptraffic");

    apply_theme_list_value(&theme, ui_lblHostnameValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblMACValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblIPValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblSSIDValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblGatewayValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblDNSValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblSignalValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblChannelValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblACTrafficValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblTPTrafficValue_netinfo, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
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
    nav_moved = 1;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHostname_netinfo || element_focused == ui_lblMAC_netinfo) {
        lv_label_set_text(ui_lblNavA, element_focused == ui_lblHostname_netinfo ?
                                      lang.GENERIC.EDIT : lang.GENERIC.CHANGE);

        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_label_set_text(ui_lblNavA, "");

        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblHostname_netinfo) {
        hostname_original = lv_label_get_text(ui_lblHostnameValue_netinfo);

        const char *new_hostname = lv_textarea_get_text(ui_txtEntry_netinfo);
        if (strlen(new_hostname) < 3) goto clear_osk;

        lv_label_set_text(ui_lblHostnameValue_netinfo, new_hostname);
        write_text_to_file("/etc/hostname", "w", CHAR, new_hostname);

        const char *hn_set_args[] = {"hostname", new_hostname, NULL};
        run_exec(hn_set_args, A_SIZE(hn_set_args), 1);
    }

    clear_osk:
    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_netinfo, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

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
    play_sound(SND_BACK, 0);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netinfo");

    close_input();
    mux_input_stop();
}

static void handle_a() {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHostname_netinfo) {
        if (is_network_connected()) {
            play_sound(SND_ERROR, 0);
            toast_message(lang.MUXNETINFO.ERROR.EDIT, 1000, 1000);
            return;
        }

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_netinfo);

        lv_textarea_set_text(ui_txtEntry_netinfo, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else if (element_focused == ui_lblMAC_netinfo) {
        if (is_network_connected()) {
            play_sound(SND_ERROR, 0);
            toast_message(lang.MUXNETINFO.ERROR.CHANGE, 1000, 1000);
            return;
        }

        const char *mac_change_args[] = {"macchanger", "-r", device.NETWORK.INTERFACE, NULL};
        run_exec(mac_change_args, A_SIZE(mac_change_args), 1);
    }
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry_netinfo, ui_pnlEntry_netinfo);
        return;
    }

    handle_back();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_backspace(ui_txtEntry_netinfo);
        return;
    }
}

static void handle_y(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }
}

static void handle_help(void) {
    if (msgbox_active || key_show) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up();
}

static void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down();
}

static void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }
}

static void handle_left_hold(void) {
    if (key_show) {
        key_left();
        return;
    }
}

static void handle_right_hold(void) {
    if (key_show) {
        key_right();
        return;
    }
}

static void handle_l1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_down();
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.EDIT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblHostname_netinfo, "hostname");
    lv_obj_set_user_data(ui_lblMAC_netinfo, "mac");
    lv_obj_set_user_data(ui_lblIP_netinfo, "ip");
    lv_obj_set_user_data(ui_lblSSID_netinfo, "ssid");
    lv_obj_set_user_data(ui_lblGateway_netinfo, "gateway");
    lv_obj_set_user_data(ui_lblDNS_netinfo, "dns");
    lv_obj_set_user_data(ui_lblSignal_netinfo, "signal");
    lv_obj_set_user_data(ui_lblChannel_netinfo, "channel");
    lv_obj_set_user_data(ui_lblACTraffic_netinfo, "actraffic");
    lv_obj_set_user_data(ui_lblTPTraffic_netinfo, "tptraffic");

    if (!is_network_connected()) {
        lv_obj_add_flag(ui_pnlIP_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlSSID_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlGateway_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlDNS_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlSignal_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlChannel_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlACTraffic_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlTPTraffic_netinfo, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

        ui_count -= 7;
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
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

int muxnetinfo_main() {
    init_module("muxnetinfo");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETINFO.TITLE);
    init_muxnetinfo(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    update_network_info();

    init_osk(ui_pnlEntry_netinfo, ui_txtEntry_netinfo, false);
    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, update_network_info);

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
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
