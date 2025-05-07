#include "muxshare.h"
#include "muxnetinfo.h"
#include "ui/ui_muxnetinfo.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

#define UI_COUNT 7
static lv_obj_t *ui_objects[UI_COUNT];
#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblIP_netinfo,      lang.MUXNETINFO.HELP.IP},
            {ui_lblMAC_netinfo,     lang.MUXNETINFO.HELP.MAC},
            {ui_lblSSID_netinfo,    lang.MUXNETINFO.HELP.SSID},
            {ui_lblGateway_netinfo, lang.MUXNETINFO.HELP.GATEWAY},
            {ui_lblDNS_netinfo,     lang.MUXNETINFO.HELP.DNS},
            {ui_lblSignal_netinfo,  lang.MUXNETINFO.HELP.SIGNAL},
            {ui_lblTraffic_netinfo, lang.MUXNETINFO.HELP.TRAFFIC},
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

static const char *get_ip_address() {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip addr show %s | awk '/inet / {print $2}' | cut -d/ -f1", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char ip[64];
    snprintf(ip, sizeof(ip), "%s", result);

    return ip;
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

static const char *get_ssid() {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link | awk -F': ' '/SSID/ {print $2}'", device.NETWORK.INTERFACE);

    const char *result = get_execute_result(cmd);
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char ssid[64];
    snprintf(ssid, sizeof(ssid), "%s", result);

    return ssid;
}

static const char *get_gateway() {
    const char *result = get_execute_result("ip route | awk '/default/ {print $3}'");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char gw[64];
    snprintf(gw, sizeof(gw), "%s", result);

    return gw;
}

static const char *get_dns_servers() {
    const char *result = get_execute_result("awk '/nameserver/ {print $2}' /etc/resolv.conf | xargs");
    if (!result || strlen(result) == 0) return lang.GENERIC.UNKNOWN;

    static char dns[128];
    snprintf(dns, sizeof(dns), "%s", result);

    return dns;
}

static const char *get_signal_strength() {
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

static const char *get_traffic() {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return lang.GENERIC.UNKNOWN;

    char line[256];
    const char *iface = device.NETWORK.INTERFACE;
    size_t iface_len = strlen(iface);

    unsigned long long rx_bytes = 0, tx_bytes = 0;

    // Skip the two headers at the top
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        char *ptr = line;
        while (*ptr == ' ') ptr++;

        if (strncmp(ptr, iface, iface_len) == 0 && ptr[iface_len] == ':') {
            ptr = strchr(ptr, ':');
            if (!ptr) break;
            ptr++;

            // RX bytes is field 1, TX bytes is field 9 - at least with busybox!
            // This gets the total traffic, couldn't work out how to get live data!
            char *token;
            int field = 0;
            while ((token = strsep(&ptr, " ")) != NULL) {
                if (*token == '\0') continue;

                field++;
                if (field == 1) {
                    rx_bytes = strtoull(token, NULL, 10);
                } else if (field == 9) {
                    tx_bytes = strtoull(token, NULL, 10);
                    break;
                }
            }

            break;
        }
    }

    fclose(fp);

    static char traffic[64];
    snprintf(traffic, sizeof(traffic), "RX: %.1f MB TX: %.1f MB",
             rx_bytes / 1024.0 / 1024.0, tx_bytes / 1024.0 / 1024.0);

    return traffic;
}

static void update_network_info() {
    lv_label_set_text(ui_lblIPValue_netinfo, get_ip_address());
    lv_label_set_text(ui_lblMACValue_netinfo, get_mac_address());
    lv_label_set_text(ui_lblSSIDValue_netinfo, get_ssid());
    lv_label_set_text(ui_lblGatewayValue_netinfo, get_gateway());
    lv_label_set_text(ui_lblDNSValue_netinfo, get_dns_servers());
    lv_label_set_text(ui_lblSignalValue_netinfo, get_signal_strength());
    lv_label_set_text(ui_lblTrafficValue_netinfo, get_traffic());
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlIP_netinfo,
            ui_pnlMAC_netinfo,
            ui_pnlSSID_netinfo,
            ui_pnlGateway_netinfo,
            ui_pnlDNS_netinfo,
            ui_pnlSignal_netinfo,
            ui_pnlTraffic_netinfo
    };

    ui_objects[0] = ui_lblIP_netinfo;
    ui_objects[1] = ui_lblMAC_netinfo;
    ui_objects[2] = ui_lblSSID_netinfo;
    ui_objects[3] = ui_lblGateway_netinfo;
    ui_objects[4] = ui_lblDNS_netinfo;
    ui_objects[5] = ui_lblSignal_netinfo;
    ui_objects[6] = ui_lblTraffic_netinfo;

    lv_obj_t *ui_objects_value[] = {
            ui_lblIPValue_netinfo,
            ui_lblMACValue_netinfo,
            ui_lblSSIDValue_netinfo,
            ui_lblGatewayValue_netinfo,
            ui_lblDNSValue_netinfo,
            ui_lblSignalValue_netinfo,
            ui_lblTrafficValue_netinfo
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoIP_netinfo,
            ui_icoMAC_netinfo,
            ui_icoSSID_netinfo,
            ui_icoGateway_netinfo,
            ui_icoDNS_netinfo,
            ui_icoSignal_netinfo,
            ui_icoTraffic_netinfo
    };

    apply_theme_list_panel(ui_pnlIP_netinfo);
    apply_theme_list_panel(ui_pnlMAC_netinfo);
    apply_theme_list_panel(ui_pnlSSID_netinfo);
    apply_theme_list_panel(ui_pnlGateway_netinfo);
    apply_theme_list_panel(ui_pnlDNS_netinfo);
    apply_theme_list_panel(ui_pnlSignal_netinfo);
    apply_theme_list_panel(ui_pnlTraffic_netinfo);

    apply_theme_list_item(&theme, ui_lblIP_netinfo, lang.MUXNETINFO.IP);
    apply_theme_list_item(&theme, ui_lblMAC_netinfo, lang.MUXNETINFO.MAC);
    apply_theme_list_item(&theme, ui_lblSSID_netinfo, lang.MUXNETINFO.SSID);
    apply_theme_list_item(&theme, ui_lblGateway_netinfo, lang.MUXNETINFO.GATEWAY);
    apply_theme_list_item(&theme, ui_lblDNS_netinfo, lang.MUXNETINFO.DNS);
    apply_theme_list_item(&theme, ui_lblSignal_netinfo, lang.MUXNETINFO.SIGNAL);
    apply_theme_list_item(&theme, ui_lblTraffic_netinfo, lang.MUXNETINFO.TRAFFIC);

    apply_theme_list_glyph(&theme, ui_icoIP_netinfo, mux_module, "ip");
    apply_theme_list_glyph(&theme, ui_icoMAC_netinfo, mux_module, "mac");
    apply_theme_list_glyph(&theme, ui_icoSSID_netinfo, mux_module, "ssid");
    apply_theme_list_glyph(&theme, ui_icoGateway_netinfo, mux_module, "gateway");
    apply_theme_list_glyph(&theme, ui_icoDNS_netinfo, mux_module, "dns");
    apply_theme_list_glyph(&theme, ui_icoSignal_netinfo, mux_module, "signal");
    apply_theme_list_glyph(&theme, ui_icoTraffic_netinfo, mux_module, "traffic");

    apply_theme_list_value(&theme, ui_lblIPValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblMACValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblSSIDValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblGatewayValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblDNSValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblSignalValue_netinfo, "");
    apply_theme_list_value(&theme, ui_lblTrafficValue_netinfo, "");

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
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (msgbox_active) return;
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netinfo");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help(lv_group_get_focused(ui_group));
    }
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblIP_netinfo, "ip");
    lv_obj_set_user_data(ui_lblMAC_netinfo, "mac");
    lv_obj_set_user_data(ui_lblSSID_netinfo, "ssid");
    lv_obj_set_user_data(ui_lblGateway_netinfo, "gateway");
    lv_obj_set_user_data(ui_lblDNS_netinfo, "dns");
    lv_obj_set_user_data(ui_lblSignal_netinfo, "signal");
    lv_obj_set_user_data(ui_lblTraffic_netinfo, "traffic");

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

int muxnetinfo_main() {
    init_module("muxnetinfo");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETINFO.TITLE);
    init_muxnetinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    update_network_info();

    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, update_network_info);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
