#include "muxshare.h"
#include "muxnetwork.h"
#include "ui/ui_muxnetwork.h"
#include <unistd.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/input/list_nav.h"

const char *pass_args[] = {(INTERNAL_PATH "script/web/password.sh"), NULL};
const char *net_c_args[] = {(INTERNAL_PATH "script/system/network.sh"), "connect", NULL};
const char *net_d_args[] = {(INTERNAL_PATH "script/system/network.sh"), "disconnect", NULL};

#define PASS_ENCODE "********"

#define UI_DHCP 5
#define UI_STATIC 9
#define UI_COUNT 9
lv_obj_t *ui_panels[UI_COUNT];
static lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_values[UI_COUNT];
static lv_obj_t *ui_icons[UI_COUNT];

static lv_obj_t *ui_mux_panels[7];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblIdentifier_network, lang.MUXNETWORK.HELP.SSID},
            {ui_lblPassword_network,   lang.MUXNETWORK.HELP.PASSWORD},
            {ui_lblScan_network,       lang.MUXNETWORK.HELP.HIDDEN},
            {ui_lblType_network,       lang.MUXNETWORK.HELP.TYPE},
            {ui_lblAddress_network,    lang.MUXNETWORK.HELP.IP},
            {ui_lblSubnet_network,     lang.MUXNETWORK.HELP.CIDR},
            {ui_lblGateway_network,    lang.MUXNETWORK.HELP.GATEWAY},
            {ui_lblConnect_network,    lang.MUXNETWORK.HELP.CONNECT},
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

static void can_scan_check(int forced_disconnect) {
    if (!forced_disconnect && is_network_connected()) {
        lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.DISCONNECT);

        lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

        update_network_status(ui_staNetwork, &theme, 0);
        return;
    }

    lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.CONNECT);

    lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

    lv_label_set_text(ui_lblConnect_networkValue, lang.MUXNETWORK.NOT_CONNECTED);

    update_network_status(ui_staNetwork, &theme, 2);
}

static void get_current_ip() {
    char address_file[MAX_BUFFER_SIZE];
    snprintf(address_file, sizeof(address_file),
             CONF_CONFIG_PATH "/network/address");

    char *curr_ip = read_all_char_from(address_file);
    static char net_message[MAX_BUFFER_SIZE];

    if (strlen(curr_ip) > 1) {
        if (!strcasecmp(curr_ip, "0.0.0.0")) {
            can_scan_check(1);
        } else {
            if (config.NETWORK.TYPE) {
                snprintf(net_message, sizeof(net_message), "%s", lang.MUXNETWORK.CONNECTED);
                lv_label_set_text(ui_lblConnect_networkValue, net_message);
            } else {
                snprintf(net_message, sizeof(net_message), "%s", curr_ip);
                lv_label_set_text(ui_lblConnect_networkValue, net_message);
            }
            lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.DISCONNECT);

            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

            update_network_status(ui_staNetwork, &theme, 1);
        }
    } else {
        can_scan_check(1);
    }
}

static void restore_network_values() {
    if (config.NETWORK.TYPE) {
        lv_label_set_text(ui_lblType_networkValue, lang.MUXNETWORK.STATIC);
        ui_count = UI_STATIC;
    } else {
        lv_label_set_text(ui_lblType_networkValue, lang.MUXNETWORK.DHCP);
        ui_count = UI_DHCP;
    }

    if (config.NETWORK.SCAN) {
        lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.ENABLED);
    } else {
        lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.DISABLED);
    }

    lv_label_set_text(ui_lblIdentifier_networkValue, config.NETWORK.SSID);
    lv_label_set_text(ui_lblPassword_networkValue, config.NETWORK.PASS);
    lv_label_set_text(ui_lblAddress_networkValue, config.NETWORK.ADDRESS);
    lv_label_set_text(ui_lblSubnet_networkValue, config.NETWORK.SUBNET);
    lv_label_set_text(ui_lblGateway_networkValue, config.NETWORK.GATEWAY);
    lv_label_set_text(ui_lblDNS_networkValue, config.NETWORK.DNS);

    if (strlen(config.NETWORK.PASS) >= 64) lv_label_set_text(ui_lblPassword_networkValue, PASS_ENCODE);

    get_current_ip();
}

static void save_network_config() {
    int idx_type = 0;
    int idx_scan = 0;

    if (!strcasecmp(lv_label_get_text(ui_lblType_networkValue), lang.MUXNETWORK.STATIC)) idx_type = 1;
    if (!strcasecmp(lv_label_get_text(ui_lblScan_networkValue), lang.GENERIC.ENABLED)) idx_scan = 1;

    write_text_to_file((CONF_CONFIG_PATH "network/type"), "w", INT, idx_type);
    write_text_to_file((CONF_CONFIG_PATH "network/scan"), "w", INT, idx_scan);
    write_text_to_file((CONF_CONFIG_PATH "network/ssid"), "w", CHAR,
                       lv_label_get_text(ui_lblIdentifier_networkValue));

    if (strcasecmp(lv_label_get_text(ui_lblPassword_networkValue), PASS_ENCODE) != 0) {
        write_text_to_file((CONF_CONFIG_PATH "network/pass"), "w", CHAR,
                           lv_label_get_text(ui_lblPassword_networkValue));
    }

    if (config.NETWORK.TYPE) {
        write_text_to_file((CONF_CONFIG_PATH "network/address"), "w", CHAR,
                           lv_label_get_text(ui_lblAddress_networkValue));
        write_text_to_file((CONF_CONFIG_PATH "network/subnet"), "w", CHAR,
                           lv_label_get_text(ui_lblSubnet_networkValue));
        write_text_to_file((CONF_CONFIG_PATH "network/gateway"), "w", CHAR,
                           lv_label_get_text(ui_lblGateway_networkValue));
        write_text_to_file((CONF_CONFIG_PATH "network/dns"), "w", CHAR,
                           lv_label_get_text(ui_lblDNS_networkValue));
    }

    refresh_config = 1;
}

static void init_navigation_group() {
    ui_panels[0] = ui_pnlIdentifier_network;
    ui_panels[1] = ui_pnlPassword_network;
    ui_panels[2] = ui_pnlScan_network;
    ui_panels[3] = ui_pnlType_network;
    ui_panels[4] = ui_pnlAddress_network;
    ui_panels[5] = ui_pnlSubnet_network;
    ui_panels[6] = ui_pnlGateway_network;
    ui_panels[7] = ui_pnlDNS_network;
    ui_panels[8] = ui_pnlConnect_network;

    ui_objects[0] = ui_lblIdentifier_network;
    ui_objects[1] = ui_lblPassword_network;
    ui_objects[2] = ui_lblScan_network;
    ui_objects[3] = ui_lblType_network;
    ui_objects[4] = ui_lblAddress_network;
    ui_objects[5] = ui_lblSubnet_network;
    ui_objects[6] = ui_lblGateway_network;
    ui_objects[7] = ui_lblDNS_network;
    ui_objects[8] = ui_lblConnect_network;

    ui_values[0] = ui_lblIdentifier_networkValue;
    ui_values[1] = ui_lblPassword_networkValue;
    ui_values[2] = ui_lblScan_networkValue;
    ui_values[3] = ui_lblType_networkValue;
    ui_values[4] = ui_lblAddress_networkValue;
    ui_values[5] = ui_lblSubnet_networkValue;
    ui_values[6] = ui_lblGateway_networkValue;
    ui_values[7] = ui_lblDNS_networkValue;
    ui_values[8] = ui_lblConnect_networkValue;

    ui_icons[0] = ui_icoIdentifier_network;
    ui_icons[1] = ui_icoPassword_network;
    ui_icons[2] = ui_icoScan_network;
    ui_icons[3] = ui_icoType_network;
    ui_icons[4] = ui_icoAddress_network;
    ui_icons[5] = ui_icoSubnet_network;
    ui_icons[6] = ui_icoGateway_network;
    ui_icons[7] = ui_icoDNS_network;
    ui_icons[8] = ui_icoConnect_network;

    apply_theme_list_panel(ui_pnlIdentifier_network);
    apply_theme_list_panel(ui_pnlPassword_network);
    apply_theme_list_panel(ui_pnlScan_network);
    apply_theme_list_panel(ui_pnlType_network);
    apply_theme_list_panel(ui_pnlAddress_network);
    apply_theme_list_panel(ui_pnlSubnet_network);
    apply_theme_list_panel(ui_pnlGateway_network);
    apply_theme_list_panel(ui_pnlDNS_network);
    apply_theme_list_panel(ui_pnlConnect_network);

    apply_theme_list_item(&theme, ui_lblIdentifier_network, lang.MUXNETWORK.SSID);
    apply_theme_list_item(&theme, ui_lblPassword_network, lang.MUXNETWORK.PASSWORD);
    apply_theme_list_item(&theme, ui_lblScan_network, lang.MUXNETWORK.HIDDEN);
    apply_theme_list_item(&theme, ui_lblType_network, lang.MUXNETWORK.TYPE);
    apply_theme_list_item(&theme, ui_lblAddress_network, lang.MUXNETWORK.IP);
    apply_theme_list_item(&theme, ui_lblSubnet_network, lang.MUXNETWORK.CIDR);
    apply_theme_list_item(&theme, ui_lblGateway_network, lang.MUXNETWORK.GATEWAY);
    apply_theme_list_item(&theme, ui_lblDNS_network, lang.MUXNETWORK.DNS);
    apply_theme_list_item(&theme, ui_lblConnect_network, lang.MUXNETWORK.CONNECT);

    apply_theme_list_glyph(&theme, ui_icoIdentifier_network, mux_module, "identifier");
    apply_theme_list_glyph(&theme, ui_icoPassword_network, mux_module, "password");
    apply_theme_list_glyph(&theme, ui_icoScan_network, mux_module, "scan");
    apply_theme_list_glyph(&theme, ui_icoType_network, mux_module, "type");
    apply_theme_list_glyph(&theme, ui_icoAddress_network, mux_module, "address");
    apply_theme_list_glyph(&theme, ui_icoSubnet_network, mux_module, "subnet");
    apply_theme_list_glyph(&theme, ui_icoGateway_network, mux_module, "gateway");
    apply_theme_list_glyph(&theme, ui_icoDNS_network, mux_module, "dns");
    apply_theme_list_glyph(&theme, ui_icoConnect_network, mux_module, "connect");

    apply_theme_list_value(&theme, ui_lblIdentifier_networkValue, "");
    apply_theme_list_value(&theme, ui_lblPassword_networkValue, "");
    apply_theme_list_value(&theme, ui_lblScan_networkValue, "");
    apply_theme_list_value(&theme, ui_lblType_networkValue, "");
    apply_theme_list_value(&theme, ui_lblAddress_networkValue, "");
    apply_theme_list_value(&theme, ui_lblSubnet_networkValue, "");
    apply_theme_list_value(&theme, ui_lblGateway_networkValue, "");
    apply_theme_list_value(&theme, ui_lblDNS_networkValue, "");
    apply_theme_list_value(&theme, ui_lblConnect_networkValue, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_values[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_panels[i]);
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
    if (element_focused == ui_lblType_network) {
        lv_obj_clear_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_add_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
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

    if (element_focused == ui_lblIdentifier_network) {
        lv_label_set_text(ui_lblIdentifier_networkValue,
                          lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblPassword_network) {
        lv_label_set_text(ui_lblPassword_networkValue,
                          lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblAddress_network) {
        lv_label_set_text(ui_lblAddress_networkValue,
                          lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblSubnet_network) {
        lv_label_set_text(ui_lblSubnet_networkValue, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblGateway_network) {
        lv_label_set_text(ui_lblGateway_networkValue,
                          lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblDNS_network) {
        lv_label_set_text(ui_lblDNS_networkValue, lv_textarea_get_text(ui_txtEntry_network));
    }

    if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
        reset_osk(num_entry);
    } else {
        reset_osk(key_entry);
    }

    lv_textarea_set_text(ui_txtEntry_network, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry_network, LV_OBJ_FLAG_HIDDEN);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    const char *is_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        is_key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
    } else {
        is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    }

    if (!strcasecmp(is_key, OSK_DONE)) {
        handle_keyboard_OK_press();
    } else if (!strcmp(is_key, OSK_UPPER)) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (!strcmp(is_key, OSK_CHAR)) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (!strcmp(is_key, OSK_LOWER)) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
        } else {
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        }
    }
}

bool handle_navigate(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblScan_network) {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_NAVIGATE, 0);
            if (!strcasecmp(lv_label_get_text(ui_lblScan_networkValue), lang.GENERIC.ENABLED)) {
                write_text_to_file((CONF_CONFIG_PATH "network/scan"), "w", INT, 0);
                lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.DISABLED);
            } else {
                write_text_to_file((CONF_CONFIG_PATH "network/scan"), "w", INT, 1);
                lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.ENABLED);
            }
        } else {
            play_sound(SND_ERROR, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000);
        }
        return true;
    } else if (element_focused == ui_lblType_network) {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_NAVIGATE, 0);
            if (!strcasecmp(lv_label_get_text(ui_lblType_networkValue), lang.MUXNETWORK.STATIC)) {
                lv_label_set_text(ui_lblType_networkValue, lang.MUXNETWORK.DHCP);
                ui_count = UI_DHCP;
                lv_obj_add_flag(ui_pnlAddress_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlSubnet_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlGateway_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlDNS_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
            } else {
                lv_label_set_text(ui_lblType_networkValue, lang.MUXNETWORK.STATIC);
                ui_count = UI_STATIC;
                lv_obj_clear_flag(ui_pnlAddress_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlSubnet_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlGateway_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlDNS_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
            }
        } else {
            play_sound(SND_ERROR, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000);
        }
        return true;
    }
    return false;
}

static void handle_confirm(void) {
    if (handle_navigate()) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblConnect_network) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM, 0);
            run_exec(net_d_args, A_SIZE(net_d_args), 0);
            can_scan_check(1);
        } else {
            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_lblIdentifier_networkValue);
            const char *cv_pass = lv_label_get_text(ui_lblPassword_networkValue);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password
            int cv_pass_ok = (!strlen(cv_pass) || (strlen(cv_pass) >= 8 && strlen(cv_pass) <= 63));

            if (!strcasecmp(lv_label_get_text(ui_lblType_networkValue), lang.MUXNETWORK.STATIC)) {
                const char *cv_address = lv_label_get_text(ui_lblAddress_networkValue);
                const char *cv_subnet = lv_label_get_text(ui_lblSubnet_networkValue);
                const char *cv_gateway = lv_label_get_text(ui_lblGateway_networkValue);
                const char *cv_dns = lv_label_get_text(ui_lblDNS_networkValue);

                if (strlen(cv_ssid) > 0 && cv_pass_ok &&
                    strlen(cv_address) > 0 && strlen(cv_subnet) > 0 &&
                    strlen(cv_gateway) > 0 && strlen(cv_dns) > 0) {
                    valid_info = 1;
                }
            } else {
                if (strlen(cv_ssid) > 0 && cv_pass_ok) {
                    valid_info = 1;
                }
            }

            if (valid_info) {
                play_sound(SND_CONFIRM, 0);
                save_network_config();

                if (strlen(cv_pass) > 0) {
                    if (strcasecmp(cv_pass, PASS_ENCODE) != 0 && strcasecmp(cv_pass, "") != 0) {
                        lv_label_set_text(ui_lblConnect_networkValue, lang.MUXNETWORK.ENCRYPT_PASSWORD);
                    }
                } else {
                    lv_label_set_text(ui_lblConnect_networkValue, lang.MUXNETWORK.NO_PASSWORD);
                }

                lv_label_set_text(ui_lblPassword_networkValue, PASS_ENCODE);
                lv_label_set_text(ui_lblConnect_networkValue, lang.MUXNETWORK.CONNECT_TRY);
                lv_task_handler();

                run_exec(pass_args, A_SIZE(pass_args), 0);
                lv_task_handler();

                run_exec(net_c_args, A_SIZE(net_c_args), 0);
                lv_task_handler();

                get_current_ip();
            } else {
                play_sound(SND_ERROR, 0);
                toast_message(lang.MUXNETWORK.CHECK, 1000);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM, 0);
            if (element_focused == ui_lblScan_network) {
                if (!strcasecmp(lv_label_get_text(ui_lblScan_networkValue), lang.GENERIC.ENABLED)) {
                    write_text_to_file((CONF_CONFIG_PATH "network/scan"), "w", INT, 0);
                    lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.DISABLED);
                } else {
                    write_text_to_file((CONF_CONFIG_PATH "network/scan"), "w", INT, 1);
                    lv_label_set_text(ui_lblScan_networkValue, lang.GENERIC.ENABLED);
                }
            } else {
                key_curr = 0;
                if (element_focused == ui_lblIdentifier_network ||
                    element_focused == ui_lblPassword_network) {
                    lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

                    lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_state(num_entry, LV_STATE_DISABLED);

                    key_show = 1;
                } else {
                    lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_state(num_entry, LV_STATE_DISABLED);

                    lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_state(key_entry, LV_STATE_DISABLED);

                    key_show = 2;
                }

                lv_obj_clear_flag(ui_pnlEntry_network, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(ui_pnlEntry_network);

                if (element_focused == ui_lblPassword_network) {
                    lv_textarea_set_text(ui_txtEntry_network, "");
                } else {
                    lv_textarea_set_text(ui_txtEntry_network, lv_label_get_text(lv_group_get_focused(ui_group_value)));
                }
            }
        } else {
            play_sound(SND_ERROR, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000);
        }
    }
}

static void handle_back(void) {
    play_sound(SND_BACK, 0);

    toast_message(lang.GENERIC.SAVING, 0);
    refresh_screen(ui_screen);

    save_network_config();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "network");

    close_input();
    mux_input_stop();
}

static void handle_scan(void) {
    if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
        play_sound(SND_CONFIRM, 0);

        save_network_config();
        load_mux("net_scan");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        close_input();
        mux_input_stop();
    }
}

static void handle_profiles(void) {
    play_sound(SND_CONFIRM, 0);

    save_network_config();
    run_exec(pass_args, A_SIZE(pass_args), 0);

    load_mux("net_profile");

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    handle_confirm();
}

static void handle_b(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
            close_osk(num_entry, ui_group, ui_txtEntry_network, ui_pnlEntry_network);
        } else {
            close_osk(key_entry, ui_group, ui_txtEntry_network, ui_pnlEntry_network);
        }
        return;
    }

    handle_back();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_backspace(ui_txtEntry_network);
        return;
    }

    handle_scan();
}

static void handle_y(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }

    handle_profiles();
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

    handle_navigate();
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    handle_navigate();
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
    ui_mux_panels[3] = ui_pnlEntry_network;
    ui_mux_panels[4] = ui_pnlProgressBrightness;
    ui_mux_panels[5] = ui_pnlProgressVolume;
    ui_mux_panels[6] = ui_pnlMessage;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.MUXNETWORK.SCAN);
    lv_label_set_text(ui_lblNavY, lang.MUXNETWORK.PROFILES);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavBGlyph,
            ui_lblNavB,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblIdentifier_network, "identifier");
    lv_obj_set_user_data(ui_lblPassword_network, "password");
    lv_obj_set_user_data(ui_lblScan_network, "scan");
    lv_obj_set_user_data(ui_lblType_network, "type");
    lv_obj_set_user_data(ui_lblAddress_network, "address");
    lv_obj_set_user_data(ui_lblSubnet_network, "subnet");
    lv_obj_set_user_data(ui_lblGateway_network, "gateway");
    lv_obj_set_user_data(ui_lblDNS_network, "dns");
    lv_obj_set_user_data(ui_lblConnect_network, "connect");

    lv_obj_clear_flag(ui_pnlIdentifier_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlPassword_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlScan_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlType_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlConnect_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);

    if (!config.NETWORK.TYPE) {
        lv_obj_add_flag(ui_pnlAddress_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlSubnet_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlGateway_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlDNS_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_clear_flag(ui_pnlAddress_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlSubnet_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlGateway_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlDNS_network, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
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
        process_key_event(&ev, ui_txtEntry_network);
    }
}

int muxnetwork_main() {
    init_module("muxnetwork");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETWORK.TITLE);
    init_muxnetwork(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_network_values();

    init_osk(ui_pnlEntry_network, ui_txtEntry_network, true);
    can_scan_check(0);
    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

    init_timer(ui_refresh_task, NULL);

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
