#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

#define PASS_ENCODE "********"

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

int progress_onscreen = -1;

int key_show = 0;
int key_curr = 0;
int key_map = 0;

char *type_dhcp;
char *type_static;
char *enabled_true;
char *enabled_false;

lv_obj_t *key_entry;
lv_obj_t *num_entry;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 9
lv_obj_t *ui_panels[UI_COUNT];
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_values[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[7];

static const char *key_lower_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
                                      "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
                                      "§", "z", "x", "c", "v", "b", "n", "m", "§", "\n",
                                      "ABC", " ", "OK", NULL
};

static const char *key_upper_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
                                      "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
                                      "§", "Z", "X", "C", "V", "B", "N", "M", "§", "\n",
                                      "!@#", " ", "OK", NULL
};

static const char *key_special_map[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
                                        "`", "~", "_", "-", "+", "=", "{", "}", "[", "]", "\n",
                                        "|", "\\", ":", ";", "\"", "'", "!", "@", "#", "\n",
                                        "§", "<", ">", ",", ".", "?", "/", "$", "§", "\n",
                                        "abc", " ", "OK", NULL
};

static const char *key_number_map[] = {"7", "8", "9", "\n",
                                       "4", "5", "6", "\n",
                                       "1", "2", "3", "\n",
                                       "0", ".", "OK", NULL};

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblEnable,     TS("Toggle the Wi-Fi network on and off")},
            {ui_lblIdentifier, TS("Enter the network identifier (SSID) here")},
            {ui_lblPassword,   TS("Enter the network password here (optional)")},
            {ui_lblType,       TS("Toggle between DHCP and Static network types")},
            {ui_lblAddress,    TS("Enter the device IP address here (Static only)")},
            {ui_lblSubnet,     TS("Enter the device Subnet (CIDR) number here (Static only)")},
            {ui_lblGateway,    TS("Enter the network gateway address here (Static only)")},
            {ui_lblConnect,    TS("Connect to the network using options entered above")},
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void can_scan_check() {
    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_true) == 0) {
        if (is_network_connected()) {
            lv_label_set_text(ui_lblConnect, TS("Disconnect"));

            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);
        } else {
            lv_label_set_text(ui_lblConnect, TS("Connect"));

            lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
            lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);

            lv_label_set_text(ui_lblConnectValue, TS("Not Connected"));
        }
    }
}

void get_current_ip() {
    if (!config.NETWORK.ENABLED) {
        lv_label_set_text(ui_lblConnectValue, TS("Network Disabled"));
        return;
    }

    char address_file[MAX_BUFFER_SIZE];
    snprintf(address_file, sizeof(address_file),
             "%s/config/address.txt", INTERNAL_PATH);

    char *curr_ip = read_text_from_file(address_file);
    static char net_message[MAX_BUFFER_SIZE];

    if (strlen(curr_ip) > 1) {
        if (strcasecmp(curr_ip, "0.0.0.0") == 0) {
            can_scan_check();
        } else {
            if (config.NETWORK.TYPE) {
                snprintf(net_message, sizeof(net_message), "%s", TS("Connected"));
                lv_label_set_text(ui_lblConnectValue, net_message);
            } else {
                snprintf(net_message, sizeof(net_message), "%s", curr_ip);
                lv_label_set_text(ui_lblConnectValue, net_message);
            }
            lv_label_set_text(ui_lblConnect, TS("Disconnect"));
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
    if (config.NETWORK.ENABLED) {
        lv_label_set_text(ui_lblEnableValue, enabled_true);
    } else {
        lv_label_set_text(ui_lblEnableValue, enabled_false);
    }

    if (config.NETWORK.TYPE) {
        lv_label_set_text(ui_lblTypeValue, type_static);
        ui_count = 9;
    } else {
        lv_label_set_text(ui_lblTypeValue, type_dhcp);
        ui_count = 5;
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
    int idx_enable = 0;
    int idx_type = 0;

    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_true) == 0) idx_enable = 1;
    if (strcasecmp(lv_label_get_text(ui_lblTypeValue), type_static) == 0) idx_type = 1;

    write_text_to_file("/run/muos/global/network/enabled", "w", INT, idx_enable);
    write_text_to_file("/run/muos/global/network/type", "w", INT, idx_type);
    write_text_to_file("/run/muos/global/network/ssid", "w", CHAR, lv_label_get_text(ui_lblIdentifierValue));

    if (strcasecmp(lv_label_get_text(ui_lblPasswordValue), PASS_ENCODE) != 0) {
        write_text_to_file("/run/muos/global/network/pass", "w", CHAR, lv_label_get_text(ui_lblPasswordValue));
    }

    write_text_to_file("/run/muos/global/network/address", "w", CHAR, lv_label_get_text(ui_lblAddressValue));
    write_text_to_file("/run/muos/global/network/subnet", "w", CHAR, lv_label_get_text(ui_lblSubnetValue));
    write_text_to_file("/run/muos/global/network/gateway", "w", CHAR, lv_label_get_text(ui_lblGatewayValue));
    write_text_to_file("/run/muos/global/network/dns", "w", CHAR, lv_label_get_text(ui_lblDNSValue));
}

void init_navigation_groups() {
    ui_panels[0] = ui_pnlEnable;
    ui_panels[1] = ui_pnlIdentifier;
    ui_panels[2] = ui_pnlPassword;
    ui_panels[3] = ui_pnlType;
    ui_panels[4] = ui_pnlAddress;
    ui_panels[5] = ui_pnlSubnet;
    ui_panels[6] = ui_pnlGateway;
    ui_panels[7] = ui_pnlDNS;
    ui_panels[8] = ui_pnlConnect;

    ui_objects[0] = ui_lblEnable;
    ui_objects[1] = ui_lblIdentifier;
    ui_objects[2] = ui_lblPassword;
    ui_objects[3] = ui_lblType;
    ui_objects[4] = ui_lblAddress;
    ui_objects[5] = ui_lblSubnet;
    ui_objects[6] = ui_lblGateway;
    ui_objects[7] = ui_lblDNS;
    ui_objects[8] = ui_lblConnect;

    ui_values[0] = ui_lblEnableValue;
    ui_values[1] = ui_lblIdentifierValue;
    ui_values[2] = ui_lblPasswordValue;
    ui_values[3] = ui_lblTypeValue;
    ui_values[4] = ui_lblAddressValue;
    ui_values[5] = ui_lblSubnetValue;
    ui_values[6] = ui_lblGatewayValue;
    ui_values[7] = ui_lblDNSValue;
    ui_values[8] = ui_lblConnectValue;

    ui_icons[0] = ui_icoEnable;
    ui_icons[1] = ui_icoIdentifier;
    ui_icons[2] = ui_icoPassword;
    ui_icons[3] = ui_icoType;
    ui_icons[4] = ui_icoAddress;
    ui_icons[5] = ui_icoSubnet;
    ui_icons[6] = ui_icoGateway;
    ui_icons[7] = ui_icoDNS;
    ui_icons[8] = ui_icoConnect;

    apply_theme_list_panel(&theme, &device, ui_pnlEnable);
    apply_theme_list_panel(&theme, &device, ui_pnlIdentifier);
    apply_theme_list_panel(&theme, &device, ui_pnlPassword);
    apply_theme_list_panel(&theme, &device, ui_pnlType);
    apply_theme_list_panel(&theme, &device, ui_pnlAddress);
    apply_theme_list_panel(&theme, &device, ui_pnlSubnet);
    apply_theme_list_panel(&theme, &device, ui_pnlGateway);
    apply_theme_list_panel(&theme, &device, ui_pnlDNS);
    apply_theme_list_panel(&theme, &device, ui_pnlConnect);

    apply_theme_list_item(&theme, ui_lblEnable, TS("Network Active"), false, true);
    apply_theme_list_item(&theme, ui_lblIdentifier, TS("Identifier"), false, true);
    apply_theme_list_item(&theme, ui_lblPassword, TS("Password"), false, true);
    apply_theme_list_item(&theme, ui_lblType, TS("Network Type"), false, true);
    apply_theme_list_item(&theme, ui_lblAddress, TS("Device IP"), false, true);
    apply_theme_list_item(&theme, ui_lblSubnet, TS("Subnet CIDR"), false, true);
    apply_theme_list_item(&theme, ui_lblGateway, TS("Gateway IP"), false, true);
    apply_theme_list_item(&theme, ui_lblDNS, TS("DNS Server"), false, true);
    apply_theme_list_item(&theme, ui_lblConnect, TS("Connect"), false, true);

    apply_theme_list_glyph(&theme, ui_icoEnable, mux_module, "enable");
    apply_theme_list_glyph(&theme, ui_icoIdentifier, mux_module, "identifier");
    apply_theme_list_glyph(&theme, ui_icoPassword, mux_module, "password");
    apply_theme_list_glyph(&theme, ui_icoType, mux_module, "type");
    apply_theme_list_glyph(&theme, ui_icoAddress, mux_module, "address");
    apply_theme_list_glyph(&theme, ui_icoSubnet, mux_module, "subnet");
    apply_theme_list_glyph(&theme, ui_icoGateway, mux_module, "gateway");
    apply_theme_list_glyph(&theme, ui_icoDNS, mux_module, "dns");
    apply_theme_list_glyph(&theme, ui_icoConnect, mux_module, "connect");

    apply_theme_list_value(&theme, ui_lblEnableValue, "");
    apply_theme_list_value(&theme, ui_lblIdentifierValue, "");
    apply_theme_list_value(&theme, ui_lblPasswordValue, "");
    apply_theme_list_value(&theme, ui_lblTypeValue, "");
    apply_theme_list_value(&theme, ui_lblAddressValue, "");
    apply_theme_list_value(&theme, ui_lblSubnetValue, "");
    apply_theme_list_value(&theme, ui_lblGatewayValue, "");
    apply_theme_list_value(&theme, ui_lblDNSValue, "");
    apply_theme_list_value(&theme, ui_lblConnectValue, "");

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

void reset_osk() {
    key_curr = 0;
    lv_btnmatrix_clear_btn_ctrl_all(key_entry, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_clear_btn_ctrl_all(num_entry, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_selected_btn(key_entry, key_curr);
    lv_btnmatrix_set_selected_btn(num_entry, key_curr);
    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(num_entry, lv_btnmatrix_get_selected_btn(num_entry), LV_BTNMATRIX_CTRL_CHECKED);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
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
        play_sound("navigate", nav_sound, 0);
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
    play_sound("navigate", nav_sound, 0);
    const char *is_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        is_key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
    } else {
        is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    }
    if (strcasecmp(is_key, "OK") == 0) {
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
        reset_osk();
        lv_textarea_set_text(ui_txtEntry, "");
        lv_group_set_focus_cb(ui_group, NULL);
        lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
    } else if (strcmp(is_key, "ABC") == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, "!@#") == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, "abc") == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
        } else {
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        }
    }
}

void handle_keyboard_close(void) {
    play_sound("navigate", nav_sound, 0);
    key_show = 0;
    reset_osk();
    lv_textarea_set_text(ui_txtEntry, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
}

void handle_keyboard_backspace(void) {
    play_sound("navigate", nav_sound, 0);
    lv_textarea_del_char(ui_txtEntry);
}

void handle_keyboard_swap(void) {
    play_sound("navigate", nav_sound, 0);
    if (key_show == 1) {
        switch (key_map) {
            case 0:
                lv_btnmatrix_set_map(key_entry, key_upper_map);
                key_map = 1;
                break;
            case 1:
                lv_btnmatrix_set_map(key_entry, key_special_map);
                key_map = 2;
                break;
            case 2:
                lv_btnmatrix_set_map(key_entry, key_lower_map);
                key_map = 0;
                break;
            default:
                break;
        }
    }
}

void handle_keyboard_up(void) {
    if (key_curr >= 1) {
        switch (key_curr) {
            case 26:
                key_curr = 17;
                break;
            case 27:
                key_curr = 18;
                break;
            case 28:
                key_curr = 19;
                break;
            case 30 ... 36:
                key_curr = key_curr - 9;
                break;
            case 38:
                key_curr = 30;
                break;
            case 39:
                key_curr = 33;
                break;
            case 40:
                key_curr = 36;
                break;
            default:
                if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                    key_curr = key_curr - 3;
                } else {
                    key_curr = key_curr - 10;
                }
                break;
        }
        if (key_curr < 0) {
            key_curr = 0;
        }
    }
    if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
        key_curr--;
    }
    lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
    lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
}

void handle_keyboard_down(void) {
    int max_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        max_key = 11;
    } else {
        max_key = 40;
    }
    if (key_curr <= max_key) {
        switch (key_curr) {
            case 17:
                key_curr = 26;
                break;
            case 18:
                key_curr = 27;
                break;
            case 19:
                key_curr = 28;
                break;
            case 21 ... 27:
                key_curr = key_curr + 9;
                break;
            case 28:
                key_curr = 36;
                break;
            case 30:
                key_curr = 38;
                break;
            case 31 ... 35:
                key_curr = 39;
                break;
            case 36:
                key_curr = 40;
                break;
            default:
                if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                    key_curr = key_curr + 3;
                } else {
                    key_curr = key_curr + 10;
                }
                break;
        }
        if (key_curr > max_key) {
            key_curr = max_key;
        }
    }
    if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
        key_curr++;
    }
    lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
    lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
}

void handle_keyboard_left(void) {
    if (key_curr >= 1) {
        key_curr--;
        if (key_curr < 0) {
            key_curr = 0;
        }
        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
            key_curr--;
        }
        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

void handle_keyboard_right(void) {
    int max_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        max_key = 11;
    } else {
        max_key = 40;
    }
    if (key_curr <= max_key) {
        key_curr++;
        if (key_curr > max_key) {
            key_curr = max_key;
        }
        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
            key_curr++;
        }
        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

bool handle_navigate(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblEnable) {
        play_sound("navigate", nav_sound, 1);
        if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_true) == 0) {
            lv_label_set_text(ui_lblEnableValue, enabled_false);
            lv_obj_add_flag(ui_pnlIdentifier, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlPassword, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlType, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlConnect, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(ui_lblEnableValue, enabled_true);
            lv_obj_clear_flag(ui_pnlIdentifier, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlPassword, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlType, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlConnect, LV_OBJ_FLAG_HIDDEN);
            if (!is_network_connected()) {
                lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
            if (strcasecmp(lv_label_get_text(ui_lblTypeValue), type_static) == 0) {
                lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return true;
    } else if (element_focused == ui_lblType) {
        play_sound("navigate", nav_sound, 1);
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            if (strcasecmp(lv_label_get_text(ui_lblTypeValue), type_static) == 0) {
                lv_label_set_text(ui_lblTypeValue, type_dhcp);
                ui_count = 5;
                lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_FLOATING);
                lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_FLOATING);
            } else {
                lv_label_set_text(ui_lblTypeValue, type_static);
                ui_count = 9;
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
            toast_message(TS("Cannot modify while connected!"), 1000, 1000);
        }
        return true;
    }
    return false;
}

void handle_confirm(void) {
    if (handle_navigate()) return;

    play_sound("confirm", nav_sound, 1);
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblConnect) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            write_text_to_file("/run/muos/global/network/enabled", "w", INT, 0);
            system("/opt/muos/script/system/network.sh");
            write_text_to_file("/run/muos/global/network/enabled", "w", INT, 1);
            can_scan_check();
        } else {
            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_lblIdentifierValue);
            const char *cv_pass = lv_label_get_text(ui_lblPasswordValue);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password
            int cv_pass_ok = (strlen(cv_pass) == 0 || (strlen(cv_pass) >= 8 && strlen(cv_pass) <= 63));

            if (strcasecmp(lv_label_get_text(ui_lblTypeValue), type_static) == 0) {
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
                save_network_config();

                if (config.NETWORK.ENABLED) {
                    if (strlen(cv_pass) > 0) {
                        if (strcasecmp(cv_pass, PASS_ENCODE) != 0 && strcasecmp(cv_pass, "") != 0) {
                            lv_label_set_text(ui_lblConnectValue, TS("Encrypting Password..."));
                        }
                    } else {
                        lv_label_set_text(ui_lblConnectValue, TS("No Password Detected..."));
                    }

                    int sec = 1024;

                    lv_label_set_text(ui_lblPasswordValue, PASS_ENCODE);
                    lv_label_set_text(ui_lblConnectValue, TS("Trying to Connect..."));
                    refresh_screen(sec);

                    system("/opt/muos/script/web/password.sh");
                    refresh_screen(sec);

                    system("/opt/muos/script/system/network.sh");
                    refresh_screen(sec);

                    get_current_ip();
                } else {
                    lv_label_set_text(ui_lblConnectValue, TS("Network Disabled"));
                    refresh_screen(device.SCREEN.WAIT);
                }
            } else {
                toast_message(TS("Please check network settings"), 1000, 1000);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
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
            if (element_focused == ui_lblPassword) {
                lv_textarea_set_text(ui_txtEntry, "");
            } else {
                lv_textarea_set_text(ui_txtEntry, lv_label_get_text(lv_group_get_focused(ui_group_value)));
            }
        } else {
            toast_message(TS("Cannot modify while connected!"), 1000, 1000);
        }
    }
}

void handle_back(void) {
    play_sound("back", nav_sound, 1);

    save_network_config();

    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_false) == 0) {
        write_text_to_file("/run/muos/global/network/enabled", "w", INT, 0);
        write_text_to_file("/run/muos/global/network/interface", "w", CHAR, device.NETWORK.INTERFACE);
        write_text_to_file("/run/muos/global/network/type", "w", INT, 0);
        write_text_to_file("/run/muos/global/network/ssid", "w", CHAR, "");
        write_text_to_file("/run/muos/global/network/pass", "w", CHAR, "");
        write_text_to_file("/run/muos/global/network/address", "w", CHAR, "192.168.0.123");
        write_text_to_file("/run/muos/global/network/subnet", "w", INT, 24);
        write_text_to_file("/run/muos/global/network/gateway", "w", CHAR, "192.168.0.1");
        write_text_to_file("/run/muos/global/network/dns", "w", CHAR, "1.1.1.1");

        system("/opt/muos/script/system/network.sh");
    }

    toast_message(TS("Changes Saved"), 1000, 1000);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "network");
    mux_input_stop();
}

void handle_scan(void) {
    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_true) == 0) {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound("confirm", nav_sound, 1);

            save_network_config();
            load_mux("net_scan");

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR,
                               lv_obj_get_user_data(lv_group_get_focused(ui_group)));

            mux_input_stop();
        }
    }
}

void handle_profiles(void) {
    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), enabled_true) == 0) {
        play_sound("confirm", nav_sound, 1);

        save_network_config();
        system("/opt/muos/script/web/password.sh");
        load_mux("net_profile");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR,
                           lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        mux_input_stop();
    }
}

void handle_a(void) {
    if (msgbox_active) {
        return;
    }

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    handle_confirm();
}

void handle_b(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 1);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        handle_keyboard_close();
        return;
    }

    handle_back();
}

void handle_x(void) {
    if (msgbox_active) {
        return;
    }

    if (key_show) {
        handle_keyboard_backspace();
        return;
    }

    handle_scan();
}

void handle_y(void) {
    if (msgbox_active) {
        return;
    }

    if (key_show) {
        handle_keyboard_swap();
        return;
    }

    handle_profiles();
}

void handle_help(void) {
    if (msgbox_active || key_show) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 1);
        show_help(lv_group_get_focused(ui_group));
    }
}

void handle_up(void) {
    if (key_show) {
        handle_keyboard_up();
        return;
    }

    handle_list_nav_up();
}

void handle_up_hold(void) {
    if (key_show) {
        handle_keyboard_up();
        return;
    }

    handle_list_nav_up_hold();
}

void handle_down(void) {
    if (key_show) {
        handle_keyboard_down();
        return;
    }

    handle_list_nav_down();
}

void handle_down_hold(void) {
    if (key_show) {
        handle_keyboard_down();
        return;
    }

    handle_list_nav_down_hold();
}

void handle_left(void) {
    if (key_show) {
        handle_keyboard_left();
        return;
    }

    handle_navigate();
}

void handle_right(void) {
    if (key_show) {
        handle_keyboard_right();
        return;
    }

    handle_navigate();
}

void handle_left_hold(void) {
    if (key_show) {
        handle_keyboard_left();
        return;
    }
}

void handle_right_hold(void) {
    if (key_show) {
        handle_keyboard_right();
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

static void osk_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(ui_txtEntry, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

static void num_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(ui_txtEntry, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

void init_elements() {
    type_dhcp = TS("DHCP");
    type_static = TS("Static");
    enabled_false = TG("Disabled");
    enabled_true = TG("Enabled");

    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlEntry;
    ui_mux_panels[4] = ui_pnlProgressBrightness;
    ui_mux_panels[5] = ui_pnlProgressVolume;
    ui_mux_panels[6] = ui_pnlMessage;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavB, TG("Back"));
    lv_label_set_text(ui_lblNavX, TS("Scan"));
    lv_label_set_text(ui_lblNavY, TS("Profiles"));

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

    lv_obj_set_user_data(ui_lblEnable, "enable");
    lv_obj_set_user_data(ui_lblIdentifier, "identifier");
    lv_obj_set_user_data(ui_lblPassword, "password");
    lv_obj_set_user_data(ui_lblType, "type");
    lv_obj_set_user_data(ui_lblAddress, "address");
    lv_obj_set_user_data(ui_lblSubnet, "subnet");
    lv_obj_set_user_data(ui_lblGateway, "gateway");
    lv_obj_set_user_data(ui_lblDNS, "dns");
    lv_obj_set_user_data(ui_lblConnect, "connect");

    if (!config.NETWORK.ENABLED) {
        lv_obj_add_flag(ui_pnlIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_pnlIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
        if (!config.NETWORK.TYPE) {
            lv_obj_clear_flag(ui_pnlAddress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlSubnet, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlGateway, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlDNS, LV_OBJ_FLAG_HIDDEN);
        }
    }

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

    if (TEST_IMAGE) display_testing_message(ui_screen);

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

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(num_entry, num_handler, LV_EVENT_ALL, NULL);

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

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
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

void direct_to_previous() {
    if (file_exist(MUOS_PDI_LOAD)) {
        char *prev = read_text_from_file(MUOS_PDI_LOAD);
        int text_hit = 0;

        for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
            const char *u_data = lv_obj_get_user_data(ui_objects[i]);

            if (strcasecmp(u_data, prev) == 0) {
                text_hit = i;
                break;
            }
        }

        if (!config.NETWORK.TYPE && strcasecmp(prev, "connect") == 0) {
            list_nav_next(4);
            nav_moved = 1;
        } else {
            if (text_hit != 0) {
                list_nav_next(text_hit);
                nav_moved = 1;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, TS("WI-FI NETWORK"));
    ui_init(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    restore_network_values();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_osk();
    direct_to_previous();
    can_scan_check();

    refresh_screen(device.SCREEN.WAIT);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
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
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
