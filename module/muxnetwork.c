#include "../lvgl/lvgl.h"
#include "ui/ui_muxnetwork.h"
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/language.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int turbo_mode = 0;
int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

const char *pass_args[] = {(INTERNAL_PATH "script/web/password.sh"), NULL};
const char *net_args[] = {(INTERNAL_PATH "script/system/network.sh"), NULL};

#define PASS_ENCODE "********"

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

int key_show = 0;
int key_curr = 0;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_DHCP 5
#define UI_STATIC 9
#define UI_COUNT 9
lv_obj_t *ui_panels[UI_COUNT];
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_values[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[7];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblIdentifier, lang.MUXNETWORK.HELP.SSID},
            {ui_lblPassword,   lang.MUXNETWORK.HELP.PASSWORD},
            {ui_lblScan,       lang.MUXNETWORK.HELP.HIDDEN},
            {ui_lblType,       lang.MUXNETWORK.HELP.TYPE},
            {ui_lblAddress,    lang.MUXNETWORK.HELP.IP},
            {ui_lblSubnet,     lang.MUXNETWORK.HELP.CIDR},
            {ui_lblGateway,    lang.MUXNETWORK.HELP.GATEWAY},
            {ui_lblConnect,    lang.MUXNETWORK.HELP.CONNECT},
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

void can_scan_check() {
    if (is_network_connected()) {
        lv_label_set_text(ui_lblConnect, lang.MUXNETWORK.DISCONNECT);

        lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);
    } else {
        lv_label_set_text(ui_lblConnect, lang.MUXNETWORK.CONNECT);

        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);

        lv_label_set_text(ui_lblConnectValue, lang.MUXNETWORK.NOT_CONNECTED);
    }
}

void get_current_ip() {
    char address_file[MAX_BUFFER_SIZE];
    snprintf(address_file, sizeof(address_file),
             "%s/config/address.txt", INTERNAL_PATH);

    char *curr_ip = read_text_from_file(address_file);
    static char net_message[MAX_BUFFER_SIZE];

    if (strlen(curr_ip) > 1) {
        if (!strcasecmp(curr_ip, "0.0.0.0")) {
            can_scan_check();
        } else {
            if (config.NETWORK.TYPE) {
                snprintf(net_message, sizeof(net_message), "%s", lang.MUXNETWORK.CONNECTED);
                lv_label_set_text(ui_lblConnectValue, net_message);
            } else {
                snprintf(net_message, sizeof(net_message), "%s", curr_ip);
                lv_label_set_text(ui_lblConnectValue, net_message);
            }
            lv_label_set_text(ui_lblConnect, lang.MUXNETWORK.DISCONNECT);
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);
        }
    } else {
        can_scan_check();
    }
}

void restore_network_values() {
    if (config.NETWORK.TYPE) {
        lv_label_set_text(ui_lblTypeValue, lang.MUXNETWORK.STATIC);
        ui_count = UI_STATIC;
    } else {
        lv_label_set_text(ui_lblTypeValue, lang.MUXNETWORK.DHCP);
        ui_count = UI_DHCP;
    }

    if (config.NETWORK.SCAN) {
        lv_label_set_text(ui_lblScanValue, lang.GENERIC.ENABLED);
    } else {
        lv_label_set_text(ui_lblScanValue, lang.GENERIC.DISABLED);
    }

    lv_label_set_text(ui_lblIdentifierValue, config.NETWORK.SSID);
    lv_label_set_text(ui_lblPasswordValue, config.NETWORK.PASS);
    lv_label_set_text(ui_lblAddressValue, config.NETWORK.ADDRESS);
    lv_label_set_text(ui_lblSubnetValue, config.NETWORK.SUBNET);
    lv_label_set_text(ui_lblGatewayValue, config.NETWORK.GATEWAY);
    lv_label_set_text(ui_lblDNSValue, config.NETWORK.DNS);

    if (strlen(config.NETWORK.PASS) >= 64) lv_label_set_text(ui_lblPasswordValue, PASS_ENCODE);

    get_current_ip();
}

void save_network_config() {
    int idx_type = 0;
    int idx_scan = 0;

    if (!strcasecmp(lv_label_get_text(ui_lblTypeValue), lang.MUXNETWORK.STATIC)) idx_type = 1;
    if (!strcasecmp(lv_label_get_text(ui_lblScanValue), lang.GENERIC.ENABLED)) idx_scan = 1;

    write_text_to_file((RUN_GLOBAL_PATH "network/type"), "w", INT, idx_type);
    write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT, idx_scan);
    write_text_to_file((RUN_GLOBAL_PATH "network/ssid"), "w", CHAR, lv_label_get_text(ui_lblIdentifierValue));

    if (strcasecmp(lv_label_get_text(ui_lblPasswordValue), PASS_ENCODE) != 0) {
        write_text_to_file((RUN_GLOBAL_PATH "network/pass"), "w", CHAR, lv_label_get_text(ui_lblPasswordValue));
    }

    write_text_to_file((RUN_GLOBAL_PATH "network/address"), "w", CHAR, lv_label_get_text(ui_lblAddressValue));
    write_text_to_file((RUN_GLOBAL_PATH "network/subnet"), "w", CHAR, lv_label_get_text(ui_lblSubnetValue));
    write_text_to_file((RUN_GLOBAL_PATH "network/gateway"), "w", CHAR, lv_label_get_text(ui_lblGatewayValue));
    write_text_to_file((RUN_GLOBAL_PATH "network/dns"), "w", CHAR, lv_label_get_text(ui_lblDNSValue));
}

void init_navigation_group() {
    ui_panels[0] = ui_pnlIdentifier;
    ui_panels[1] = ui_pnlPassword;
    ui_panels[2] = ui_pnlScan;
    ui_panels[3] = ui_pnlType;
    ui_panels[4] = ui_pnlAddress;
    ui_panels[5] = ui_pnlSubnet;
    ui_panels[6] = ui_pnlGateway;
    ui_panels[7] = ui_pnlDNS;
    ui_panels[8] = ui_pnlConnect;

    ui_objects[0] = ui_lblIdentifier;
    ui_objects[1] = ui_lblPassword;
    ui_objects[2] = ui_lblScan;
    ui_objects[3] = ui_lblType;
    ui_objects[4] = ui_lblAddress;
    ui_objects[5] = ui_lblSubnet;
    ui_objects[6] = ui_lblGateway;
    ui_objects[7] = ui_lblDNS;
    ui_objects[8] = ui_lblConnect;

    ui_values[0] = ui_lblIdentifierValue;
    ui_values[1] = ui_lblPasswordValue;
    ui_values[2] = ui_lblScanValue;
    ui_values[3] = ui_lblTypeValue;
    ui_values[4] = ui_lblAddressValue;
    ui_values[5] = ui_lblSubnetValue;
    ui_values[6] = ui_lblGatewayValue;
    ui_values[7] = ui_lblDNSValue;
    ui_values[8] = ui_lblConnectValue;

    ui_icons[0] = ui_icoIdentifier;
    ui_icons[1] = ui_icoPassword;
    ui_icons[2] = ui_icoScan;
    ui_icons[3] = ui_icoType;
    ui_icons[4] = ui_icoAddress;
    ui_icons[5] = ui_icoSubnet;
    ui_icons[6] = ui_icoGateway;
    ui_icons[7] = ui_icoDNS;
    ui_icons[8] = ui_icoConnect;

    apply_theme_list_panel(ui_pnlIdentifier);
    apply_theme_list_panel(ui_pnlPassword);
    apply_theme_list_panel(ui_pnlScan);
    apply_theme_list_panel(ui_pnlType);
    apply_theme_list_panel(ui_pnlAddress);
    apply_theme_list_panel(ui_pnlSubnet);
    apply_theme_list_panel(ui_pnlGateway);
    apply_theme_list_panel(ui_pnlDNS);
    apply_theme_list_panel(ui_pnlConnect);

    apply_theme_list_item(&theme, ui_lblIdentifier, lang.MUXNETWORK.SSID);
    apply_theme_list_item(&theme, ui_lblPassword, lang.MUXNETWORK.PASSWORD);
    apply_theme_list_item(&theme, ui_lblScan, lang.MUXNETWORK.HIDDEN);
    apply_theme_list_item(&theme, ui_lblType, lang.MUXNETWORK.TYPE);
    apply_theme_list_item(&theme, ui_lblAddress, lang.MUXNETWORK.IP);
    apply_theme_list_item(&theme, ui_lblSubnet, lang.MUXNETWORK.CIDR);
    apply_theme_list_item(&theme, ui_lblGateway, lang.MUXNETWORK.GATEWAY);
    apply_theme_list_item(&theme, ui_lblDNS, lang.MUXNETWORK.DNS);
    apply_theme_list_item(&theme, ui_lblConnect, lang.MUXNETWORK.CONNECT);

    apply_theme_list_glyph(&theme, ui_icoIdentifier, mux_module, "identifier");
    apply_theme_list_glyph(&theme, ui_icoPassword, mux_module, "password");
    apply_theme_list_glyph(&theme, ui_icoScan, mux_module, "scan");
    apply_theme_list_glyph(&theme, ui_icoType, mux_module, "type");
    apply_theme_list_glyph(&theme, ui_icoAddress, mux_module, "address");
    apply_theme_list_glyph(&theme, ui_icoSubnet, mux_module, "subnet");
    apply_theme_list_glyph(&theme, ui_icoGateway, mux_module, "gateway");
    apply_theme_list_glyph(&theme, ui_icoDNS, mux_module, "dns");
    apply_theme_list_glyph(&theme, ui_icoConnect, mux_module, "connect");

    apply_theme_list_value(&theme, ui_lblIdentifierValue, "");
    apply_theme_list_value(&theme, ui_lblPasswordValue, "");
    apply_theme_list_value(&theme, ui_lblScanValue, "");
    apply_theme_list_value(&theme, ui_lblTypeValue, "");
    apply_theme_list_value(&theme, ui_lblAddressValue, "");
    apply_theme_list_value(&theme, ui_lblSubnetValue, "");
    apply_theme_list_value(&theme, ui_lblGatewayValue, "");
    apply_theme_list_value(&theme, ui_lblDNSValue, "");
    apply_theme_list_value(&theme, ui_lblConnectValue, "");

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

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = !current_item_index ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_keyboard_press(void) {
    play_sound("navigate", nav_sound, 0, 0);

    const char *is_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        is_key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
    } else {
        is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    }

    if (!strcasecmp(is_key, OSK_DONE)) {
        key_show = 0;
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

        if (element_focused == ui_lblIdentifier) {
            lv_label_set_text(ui_lblIdentifierValue,
                              lv_textarea_get_text(ui_txtEntry));
        } else if (element_focused == ui_lblPassword) {
            lv_label_set_text(ui_lblPasswordValue,
                              lv_textarea_get_text(ui_txtEntry));
        } else if (element_focused == ui_lblAddress) {
            lv_label_set_text(ui_lblAddressValue,
                              lv_textarea_get_text(ui_txtEntry));
        } else if (element_focused == ui_lblSubnet) {
            lv_label_set_text(ui_lblSubnetValue, lv_textarea_get_text(ui_txtEntry));
        } else if (element_focused == ui_lblGateway) {
            lv_label_set_text(ui_lblGatewayValue,
                              lv_textarea_get_text(ui_txtEntry));
        } else if (element_focused == ui_lblDNS) {
            lv_label_set_text(ui_lblDNSValue, lv_textarea_get_text(ui_txtEntry));
        }

        if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
            reset_osk(num_entry);
        } else {
            reset_osk(key_entry);
        }

        lv_textarea_set_text(ui_txtEntry, "");
        lv_group_set_focus_cb(ui_group, NULL);
        lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
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
    if (element_focused == ui_lblScan) {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound("navigate", nav_sound, 0, 0);
            if (!strcasecmp(lv_label_get_text(ui_lblScanValue), lang.GENERIC.ENABLED)) {
                write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT, 0);
                lv_label_set_text(ui_lblScanValue, lang.GENERIC.DISABLED);
            } else {
                write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT, 1);
                lv_label_set_text(ui_lblScanValue, lang.GENERIC.ENABLED);
            }
        } else {
            play_sound("error", nav_sound, 0, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000, 1000);
        }
        return true;
    } else if (element_focused == ui_lblType) {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound("navigate", nav_sound, 0, 0);
            if (!strcasecmp(lv_label_get_text(ui_lblTypeValue), lang.MUXNETWORK.STATIC)) {
                lv_label_set_text(ui_lblTypeValue, lang.MUXNETWORK.DHCP);
                ui_count = UI_DHCP;
                lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_FLOATING);
            } else {
                lv_label_set_text(ui_lblTypeValue, lang.MUXNETWORK.STATIC);
                ui_count = UI_STATIC;
                lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_FLOATING);
                lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_FLOATING);
            }
        } else {
            play_sound("error", nav_sound, 0, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000, 1000);
        }
        return true;
    }
    return false;
}

void handle_confirm(void) {
    if (handle_navigate()) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblConnect) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound("confirm", nav_sound, 0, 0);
            run_exec(net_args);
            can_scan_check();
        } else {
            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_lblIdentifierValue);
            const char *cv_pass = lv_label_get_text(ui_lblPasswordValue);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password
            int cv_pass_ok = (!strlen(cv_pass) || (strlen(cv_pass) >= 8 && strlen(cv_pass) <= 63));

            if (!strcasecmp(lv_label_get_text(ui_lblTypeValue), lang.MUXNETWORK.STATIC)) {
                const char *cv_address = lv_label_get_text(ui_lblAddressValue);
                const char *cv_subnet = lv_label_get_text(ui_lblSubnetValue);
                const char *cv_gateway = lv_label_get_text(ui_lblGatewayValue);
                const char *cv_dns = lv_label_get_text(ui_lblDNSValue);

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
                play_sound("confirm", nav_sound, 0, 0);
                save_network_config();

                if (strlen(cv_pass) > 0) {
                    if (strcasecmp(cv_pass, PASS_ENCODE) != 0 && strcasecmp(cv_pass, "") != 0) {
                        lv_label_set_text(ui_lblConnectValue, lang.MUXNETWORK.ENCRYPT_PASSWORD);
                    }
                } else {
                    lv_label_set_text(ui_lblConnectValue, lang.MUXNETWORK.NO_PASSWORD);
                }

                lv_label_set_text(ui_lblPasswordValue, PASS_ENCODE);
                lv_label_set_text(ui_lblConnectValue, lang.MUXNETWORK.CONNECT_TRY);
                lv_task_handler();

                run_exec(pass_args);
                lv_task_handler();

                run_exec(net_args);
                lv_task_handler();

                get_current_ip();

            } else {
                play_sound("error", nav_sound, 0, 0);
                toast_message(lang.MUXNETWORK.CHECK, 1000, 1000);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound("confirm", nav_sound, 0, 0);
            if (element_focused == ui_lblScan) {
                if (!strcasecmp(lv_label_get_text(ui_lblScanValue), lang.GENERIC.ENABLED)) {
                    write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT, 0);
                    lv_label_set_text(ui_lblScanValue, lang.GENERIC.DISABLED);
                } else {
                    write_text_to_file((RUN_GLOBAL_PATH "network/scan"), "w", INT, 1);
                    lv_label_set_text(ui_lblScanValue, lang.GENERIC.ENABLED);
                }
            } else {
                key_curr = 0;
                if (element_focused == ui_lblIdentifier ||
                    element_focused == ui_lblPassword) {
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

                lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(ui_pnlEntry);

                if (element_focused == ui_lblPassword) {
                    lv_textarea_set_text(ui_txtEntry, "");
                } else {
                    lv_textarea_set_text(ui_txtEntry, lv_label_get_text(lv_group_get_focused(ui_group_value)));
                }
            }
        } else {
            play_sound("error", nav_sound, 0, 0);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, 1000, 1000);
        }
    }
}

void handle_back(void) {
    play_sound("back", nav_sound, 0, 1);

    save_network_config();
    toast_message(lang.MUXNETWORK.SAVE, 1000, 1000);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "network");
    mux_input_stop();
}

void handle_scan(void) {
    if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
        play_sound("confirm", nav_sound, 0, 1);

        save_network_config();
        load_mux("net_scan");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        mux_input_stop();
    }
}

void handle_profiles(void) {
    play_sound("confirm", nav_sound, 0, 1);

    save_network_config();
    run_exec(pass_args);

    load_mux("net_profile");

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

    mux_input_stop();
}

void handle_a(void) {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    handle_confirm();
}

void handle_b(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
            close_osk(num_entry, ui_group, ui_txtEntry, ui_pnlEntry);
        } else {
            close_osk(key_entry, ui_group, ui_txtEntry, ui_pnlEntry);
        }
        return;
    }

    handle_back();
}

void handle_x(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_backspace(ui_txtEntry);
        return;
    }

    handle_scan();
}

void handle_y(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }

    handle_profiles();
}

void handle_help(void) {
    if (msgbox_active || key_show) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up();
}

void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up_hold();
}

void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down();
}

void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down_hold();
}

void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    handle_navigate();
}

void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    handle_navigate();
}

void handle_left_hold(void) {
    if (key_show) {
        key_left();
        return;
    }
}

void handle_right_hold(void) {
    if (key_show) {
        key_right();
        return;
    }
}

void handle_l1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_up();
}

void handle_r1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_down();
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlEntry;
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.MUXNETWORK.SCAN);
    lv_label_set_text(ui_lblNavY, lang.MUXNETWORK.PROFILES);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblIdentifier, "identifier");
    lv_obj_set_user_data(ui_lblPassword, "password");
    lv_obj_set_user_data(ui_lblScan, "scan");
    lv_obj_set_user_data(ui_lblType, "type");
    lv_obj_set_user_data(ui_lblAddress, "address");
    lv_obj_set_user_data(ui_lblSubnet, "subnet");
    lv_obj_set_user_data(ui_lblGateway, "gateway");
    lv_obj_set_user_data(ui_lblDNS, "dns");
    lv_obj_set_user_data(ui_lblConnect, "connect");

    lv_obj_clear_flag(ui_pnlIdentifier, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlPassword, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlScan, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlType, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlConnect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);

    if (!config.NETWORK.TYPE) {
        lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_FLOATING);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void init_osk() {
    key_entry = lv_btnmatrix_create(ui_pnlEntry);
    num_entry = lv_btnmatrix_create(ui_pnlEntry);

    lv_obj_set_width(key_entry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(key_entry, device.MUX.HEIGHT * 5 / 9);
    lv_obj_set_width(num_entry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(num_entry, device.MUX.HEIGHT * 5 / 9);

    lv_btnmatrix_set_one_checked(key_entry, 1);
    lv_btnmatrix_set_one_checked(num_entry, 1);

    lv_btnmatrix_set_map(key_entry, key_lower_map);
    lv_btnmatrix_set_map(num_entry, key_number_map);

    lv_btnmatrix_set_btn_ctrl(key_entry, 29, LV_BTNMATRIX_CTRL_HIDDEN);
    lv_btnmatrix_set_btn_ctrl(key_entry, 37, LV_BTNMATRIX_CTRL_HIDDEN);

    lv_btnmatrix_set_btn_width(key_entry, 39, 3);

    lv_btnmatrix_set_selected_btn(key_entry, key_curr);
    lv_btnmatrix_set_selected_btn(num_entry, key_curr);

    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(num_entry, lv_btnmatrix_get_selected_btn(num_entry), LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_align(key_entry, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(num_entry, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_ALL, ui_txtEntry);
    lv_obj_add_event_cb(num_entry, osk_handler, LV_EVENT_ALL, ui_txtEntry);

    lv_obj_set_style_border_width(key_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(key_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(num_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(num_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(key_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(num_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                              LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_bg_opa(key_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_opa(key_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_radius(key_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(num_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_pad_top(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_height(ui_txtEntry, 48);
    lv_obj_set_style_text_color(ui_txtEntry, lv_color_hex(theme.OSK.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_txtEntry, theme.OSK.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_txtEntry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_txtEntry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETWORK.TITLE);
    init_mux(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    restore_network_values();
    init_navigation_sound(&nav_sound, mux_module);

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

    init_osk();
    can_scan_check();
    load_kiosk(&kiosk);
    list_nav_next(direct_to_previous(ui_objects, UI_COUNT, &nav_moved));

    mux_input_options input_opts = {
            .general_fd = joy_general,
            .power_fd = joy_power,
            .volume_fd = joy_volume,
            .extra_fd = joy_extra,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
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
            },
            .combo = {
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP)),
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_UP)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_DOWN)),
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);
    safe_quit();

    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);

    return 0;
}
