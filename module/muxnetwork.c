#include "muxshare.h"
#include "ui/ui_muxnetwork.h"

#define UI_COUNT 9

const char *pass_args[] = {(OPT_PATH "script/web/password.sh"), NULL};
const char *net_c_args[] = {(OPT_PATH "script/system/network.sh"), "connect", NULL};
const char *net_d_args[] = {(OPT_PATH "script/system/network.sh"), "disconnect", NULL};

#define PASS_ENCODE "********"

#define UI_DHCP (UI_COUNT - 4)
#define UI_STATIC UI_COUNT
#define NET_STATUS_FILE "/run/muos/network.status"

bool ui_network_locked = false;
static char last_status[64] = "";

static void list_nav_move(int steps, int direction);

enum connect_status {
    STATUS_ASSOCIATING,
    STATUS_AUTHENTICATING,
    STATUS_WAITING_IP,
    STATUS_VALIDATING,
    STATUS_CONNECTED,
    STATUS_FAILED,

    // These are worse! Try not to get these...
    STATUS_INVALID_PASSWORD,
    STATUS_AP_NOT_FOUND,
    STATUS_AUTH_TIMEOUT,
    STATUS_DHCP_FAILED,
    STATUS_LINK_TIMEOUT,
    STATUS_WPA_START_FAILED
};

static const struct {
    const char *status;
    int label_id;
} net_status[] = {
        {"ASSOCIATING",      STATUS_ASSOCIATING},
        {"AUTHENTICATING",   STATUS_AUTHENTICATING},
        {"WAITING_IP",       STATUS_WAITING_IP},
        {"VALIDATING",       STATUS_VALIDATING},
        {"CONNECTED",        STATUS_CONNECTED},
        {"FAILED",           STATUS_FAILED},
        {"INVALID_PASSWORD", STATUS_INVALID_PASSWORD},
        {"AP_NOT_FOUND",     STATUS_AP_NOT_FOUND},
        {"AUTH_TIMEOUT",     STATUS_AUTH_TIMEOUT},
        {"DHCP_FAILED",      STATUS_DHCP_FAILED},
        {"LINK_TIMEOUT",     STATUS_LINK_TIMEOUT},
        {"WPA_START_FAILED", STATUS_WPA_START_FAILED},
};

static void show_help(void) {
    struct help_msg help_messages[] = {
#define NETWORK(NAME, ENUM, UDATA) { UDATA, lang.MUXNETWORK.HELP.ENUM },
            NETWORK_ELEMENTS
#undef NETWORK
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static const char *net_status_label(int id) {
    switch (id) {
        case STATUS_ASSOCIATING:
            return lang.MUXNETWORK.STATUS.ASSOCIATING;
        case STATUS_AUTHENTICATING:
            return lang.MUXNETWORK.STATUS.AUTHENTICATING;
        case STATUS_WAITING_IP:
            return lang.MUXNETWORK.STATUS.WAITING_IP;
        case STATUS_VALIDATING:
            return lang.MUXNETWORK.STATUS.VALIDATING;
        case STATUS_CONNECTED:
            return lang.MUXNETWORK.CONNECTED;
        case STATUS_FAILED:
            return lang.MUXNETWORK.NOT_CONNECTED;
        case STATUS_INVALID_PASSWORD:
            return lang.MUXNETWORK.STATUS.INVALID_PASSWORD;
        case STATUS_AP_NOT_FOUND:
            return lang.MUXNETWORK.STATUS.AP_NOT_FOUND;
        case STATUS_AUTH_TIMEOUT:
            return lang.MUXNETWORK.STATUS.AUTH_TIMEOUT;
        case STATUS_DHCP_FAILED:
            return lang.MUXNETWORK.STATUS.DHCP_FAILED;
        case STATUS_LINK_TIMEOUT:
            return lang.MUXNETWORK.STATUS.LINK_TIMEOUT;
        case STATUS_WPA_START_FAILED:
            return lang.MUXNETWORK.STATUS.WPA_START_FAILED;
        default:
            return lang.GENERIC.UNKNOWN;
    }
}

static void update_network_label(void) {
    if (!ui_network_locked) return;

    if (!file_exist(NET_STATUS_FILE)) {
        lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETWORK.CONNECT_TRY);
        return;
    }

    char *status = read_line_char_from(NET_STATUS_FILE, 1);
    if (!status || !*status || strcmp(status, last_status) == 0) {
        free(status);
        return;
    }

    strncpy(last_status, status, sizeof(last_status) - 1);
    last_status[sizeof(last_status) - 1] = '\0';

    for (size_t i = 0; i < A_SIZE(net_status); i++) {
        if (strcmp(status, net_status[i].status) == 0) {
            lv_label_set_text(ui_lblConnectValue_network, net_status_label(net_status[i].label_id));
            break;
        }
    }

    free(status);
}

static void can_scan_check(int forced_disconnect) {
    if (!forced_disconnect && is_network_connected()) {
        lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.DISCONNECT);

        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

        update_network_status(ui_staNetwork, &theme, 0);
        return;
    }

    lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.CONNECT);

    lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETWORK.NOT_CONNECTED);

    update_network_status(ui_staNetwork, &theme, 2);
}

static void get_current_ip(void) {
    char address_file[MAX_BUFFER_SIZE];
    snprintf(address_file, sizeof(address_file),
             CONF_CONFIG_PATH "/network/address");

    char *curr_ip = read_all_char_from(address_file);

    if (strlen(curr_ip) > 1) {
        if (strcasecmp(curr_ip, "0.0.0.0") == 0) {
            can_scan_check(1);
        } else {
            lv_label_set_text(ui_lblConnectValue_network, config.NETWORK.TYPE ? lang.MUXNETWORK.CONNECTED : curr_ip);
            lv_label_set_text(ui_lblConnect_network, lang.MUXNETWORK.DISCONNECT);

            lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);

            update_network_status(ui_staNetwork, &theme, 1);
        }
    } else {
        can_scan_check(1);
    }
}

static void net_connect_check() {
    ui_network_locked = false;
    get_current_ip();
}

static void restore_network_values(void) {
    lv_label_set_text(ui_lblTypeValue_network, config.NETWORK.TYPE ? lang.MUXNETWORK.STATIC : lang.MUXNETWORK.DHCP);
    ui_count = config.NETWORK.TYPE ? UI_STATIC : UI_DHCP;

    lv_label_set_text(ui_lblHiddenValue_network, config.NETWORK.HIDDEN ? lang.GENERIC.ENABLED : lang.GENERIC.DISABLED);
    lv_label_set_text(ui_lblIdentifierValue_network, config.NETWORK.SSID);
    lv_label_set_text(ui_lblPasswordValue_network, config.NETWORK.PASS);
    lv_label_set_text(ui_lblAddressValue_network, config.NETWORK.ADDRESS);
    lv_label_set_text(ui_lblSubnetValue_network, config.NETWORK.SUBNET);
    lv_label_set_text(ui_lblGatewayValue_network, config.NETWORK.GATEWAY);
    lv_label_set_text(ui_lblDnsValue_network, config.NETWORK.DNS);

    if (strlen(config.NETWORK.PASS) >= 64) lv_label_set_text(ui_lblPasswordValue_network, PASS_ENCODE);

    get_current_ip();
}

static void save_network_config(void) {
    int idx_type = 0;
    int idx_hidden = 0;

    if (strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETWORK.STATIC) == 0) idx_type = 1;
    if (strcasecmp(lv_label_get_text(ui_lblHiddenValue_network), lang.GENERIC.ENABLED) == 0) idx_hidden = 1;

    write_text_to_file(CONF_CONFIG_PATH "network/type", "w", INT, idx_type);
    write_text_to_file(CONF_CONFIG_PATH "network/ssid", "w", CHAR, lv_label_get_text(ui_lblIdentifierValue_network));
    write_text_to_file(CONF_CONFIG_PATH "network/hidden", "w", INT, idx_hidden);

    if (strcasecmp(lv_label_get_text(ui_lblPasswordValue_network), PASS_ENCODE) != 0) {
        write_text_to_file(CONF_CONFIG_PATH "network/pass", "w", CHAR, lv_label_get_text(ui_lblPasswordValue_network));
    }

    if (config.NETWORK.TYPE) {
        write_text_to_file(CONF_CONFIG_PATH "network/address", "w", CHAR, lv_label_get_text(ui_lblAddressValue_network));
        write_text_to_file(CONF_CONFIG_PATH "network/subnet", "w", CHAR, lv_label_get_text(ui_lblSubnetValue_network));
        write_text_to_file(CONF_CONFIG_PATH "network/gateway", "w", CHAR, lv_label_get_text(ui_lblGatewayValue_network));
        write_text_to_file(CONF_CONFIG_PATH "network/dns", "w", CHAR, lv_label_get_text(ui_lblDnsValue_network));
    }

    refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, network, Identifier, lang.MUXNETWORK.IDENTIFIER, "identifier", "");
    INIT_VALUE_ITEM(-1, network, Password, lang.MUXNETWORK.PASSWORD, "password", "");
    INIT_VALUE_ITEM(-1, network, Hidden, lang.MUXNETWORK.HIDDEN, "hidden", "");
    INIT_VALUE_ITEM(-1, network, Type, lang.MUXNETWORK.TYPE, "type", "");
    INIT_VALUE_ITEM(-1, network, Address, lang.MUXNETWORK.ADDRESS, "address", "");
    INIT_VALUE_ITEM(-1, network, Subnet, lang.MUXNETWORK.SUBNET, "subnet", "");
    INIT_VALUE_ITEM(-1, network, Gateway, lang.MUXNETWORK.GATEWAY, "gateway", "");
    INIT_VALUE_ITEM(-1, network, Dns, lang.MUXNETWORK.DNS, "dns", "");
    INIT_VALUE_ITEM(-1, network, Connect, lang.MUXNETWORK.CONNECT, "connect", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void check_focus() {
    if (!is_network_connected()) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

        if (element_focused == ui_lblType_network) {
            lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    } else {
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
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

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblIdentifier_network) {
        lv_label_set_text(ui_lblIdentifierValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblPassword_network) {
        lv_label_set_text(ui_lblPasswordValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblAddress_network) {
        lv_label_set_text(ui_lblAddressValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblSubnet_network) {
        lv_label_set_text(ui_lblSubnetValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblGateway_network) {
        lv_label_set_text(ui_lblGatewayValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (element_focused == ui_lblDns_network) {
        lv_label_set_text(ui_lblDnsValue_network, lv_textarea_get_text(ui_txtEntry_network));
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
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    const char *is_key = lv_btnmatrix_get_btn_text(
            lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN) ? num_entry : key_entry, key_curr);

    if (strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else if (strcmp(is_key, OSK_UPPER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, OSK_CHAR) == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, OSK_LOWER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
        } else {
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        }
    }
}

static void toggle_static_panels(int is_static) {
    lv_obj_t *panels[] = {
            ui_pnlAddress_network,
            ui_pnlSubnet_network,
            ui_pnlGateway_network,
            ui_pnlDns_network,
            NULL
    };

    for (int i = 0; panels[i]; i++) {
        is_static ? lv_obj_add_flag(panels[i], MU_OBJ_FLAG_HIDE_FLOAT)
                  : lv_obj_clear_flag(panels[i], MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void toggle_option(lv_obj_t *element, const char *config_path) {
    const char *current = lv_label_get_text(element);
    int is_enabled = strcasecmp(current, lang.GENERIC.ENABLED) == 0;

    write_text_to_file(config_path, "w", INT, is_enabled ? 0 : 1);
    lv_label_set_text(element, is_enabled ? lang.GENERIC.DISABLED : lang.GENERIC.ENABLED);
}

int handle_navigate(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblHidden_network ||
        element_focused == ui_lblType_network) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, SHORT);

            return 1;
        }

        play_sound(SND_OPTION);

        if (element_focused == ui_lblHidden_network) {
            toggle_option(ui_lblHiddenValue_network, CONF_CONFIG_PATH "network/hidden");
        } else if (element_focused == ui_lblType_network) {
            int is_static = strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETWORK.STATIC) == 0;
            lv_label_set_text(ui_lblTypeValue_network, is_static ? lang.MUXNETWORK.DHCP : lang.MUXNETWORK.STATIC);
            ui_count = is_static ? UI_DHCP : UI_STATIC;
            toggle_static_panels(is_static);
        }

        return 1;
    }

    return 0;
}

static void handle_confirm(void) {
    if (handle_navigate()) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblConnect_network) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM);
            run_exec(net_d_args, A_SIZE(net_d_args), 0, 0, NULL, NULL);
            can_scan_check(1);
        } else {
            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_lblIdentifierValue_network);
            const char *cv_pass = lv_label_get_text(ui_lblPasswordValue_network);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password
            int cv_pass_ok = (strlen(cv_pass) == 0 || (strlen(cv_pass) >= 8 && strlen(cv_pass) <= 63));

            if (strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETWORK.STATIC) == 0) {
                const char *cv_address = lv_label_get_text(ui_lblAddressValue_network);
                const char *cv_subnet = lv_label_get_text(ui_lblSubnetValue_network);
                const char *cv_gateway = lv_label_get_text(ui_lblGatewayValue_network);
                const char *cv_dns = lv_label_get_text(ui_lblDnsValue_network);

                if (strlen(cv_ssid) > 0 && cv_pass_ok &&
                    strlen(cv_address) > 0 && strlen(cv_subnet) > 0 &&
                    strlen(cv_gateway) > 0 && strlen(cv_dns) > 0) {
                    valid_info = 1;
                }
            } else {
                if (strlen(cv_ssid) > 0 && cv_pass_ok) valid_info = 1;
            }

            if (valid_info) {
                play_sound(SND_CONFIRM);
                save_network_config();

                if (strlen(cv_pass) > 0) {
                    if (strcasecmp(cv_pass, PASS_ENCODE) != 0 && strcasecmp(cv_pass, "") != 0) {
                        lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETWORK.ENCRYPT_PASSWORD);
                    }
                } else {
                    lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETWORK.NO_PASSWORD);
                }

                lv_label_set_text(ui_lblPasswordValue_network, PASS_ENCODE);
                lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETWORK.CONNECT_TRY);
                lv_task_handler();

                ui_network_locked = true;
                run_exec(pass_args, A_SIZE(pass_args), 0, 0, NULL, NULL);
                lv_task_handler();

                last_status[0] = '\0';
                run_exec(net_c_args, A_SIZE(net_c_args), 1, 0, NULL, net_connect_check);
                lv_task_handler();
            } else {
                play_sound(SND_ERROR);
                toast_message(lang.MUXNETWORK.CHECK, SHORT);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM);
            if (element_focused == ui_lblHidden_network) {
                toggle_option(ui_lblHiddenValue_network, CONF_CONFIG_PATH "network/hidden");
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

                lv_textarea_set_text(ui_txtEntry_network, element_focused == ui_lblPassword_network ? "" :
                                                          lv_label_get_text(lv_group_get_focused(ui_group_value)));
            }
        } else {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETWORK.DENY_MODIFY, SHORT);
        }
    }
}

static void handle_back(void) {
    if (ui_network_locked) return;

    play_sound(SND_BACK);

    toast_message(lang.GENERIC.SAVING, FOREVER);

    save_network_config();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "network");

    close_input();
    mux_input_stop();
}

static void handle_scan(void) {
    if (ui_network_locked) return;

    if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
        play_sound(SND_CONFIRM);

        save_network_config();
        load_mux("net_scan");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        close_input();
        mux_input_stop();
    }
}

static void handle_profiles(void) {
    if (ui_network_locked) return;

    play_sound(SND_CONFIRM);

    save_network_config();
    run_exec(pass_args, A_SIZE(pass_args), 0, 1, NULL, NULL);

    load_mux("net_profile");

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call || ui_network_locked) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_b(void) {
    if (hold_call || ui_network_locked) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        close_osk(lv_obj_has_state(key_entry, LV_STATE_DISABLED) ? num_entry :
                  key_entry, ui_group, ui_txtEntry_network, ui_pnlEntry_network);
        return;
    }

    handle_back();
}

static void handle_x(void) {
    if (msgbox_active || hold_call || ui_network_locked) return;

    key_show ? key_backspace(ui_txtEntry_network) : handle_scan();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || ui_network_locked) return;

    key_show ? key_swap() : handle_profiles();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call || ui_network_locked) return;

    play_sound(SND_INFO_OPEN);
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
    if (ui_network_locked) return;

    key_show ? key_left() : handle_navigate();
}

static void handle_right(void) {
    if (ui_network_locked) return;

    key_show ? key_right() : handle_navigate();
}

static void handle_left_hold(void) {
    if (ui_network_locked) return;

    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (ui_network_locked) return;

    if (key_show) key_right();
}

static void handle_l1(void) {
    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (!key_show) handle_list_nav_page_down();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlEntry_network,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                       0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE,      0},
            {ui_lblNavBGlyph,  "",                       0},
            {ui_lblNavB,       lang.GENERIC.BACK,        0},
            {ui_lblNavXGlyph,  "",                       0},
            {ui_lblNavX,       lang.MUXNETWORK.SCAN,     0},
            {ui_lblNavYGlyph,  "",                       0},
            {ui_lblNavY,       lang.MUXNETWORK.PROFILES, 0},
            {NULL, NULL,                                 0}
    });

    lv_obj_t *connect_items[] = {
            ui_pnlIdentifier_network,
            ui_pnlPassword_network,
            ui_pnlHidden_network,
            ui_pnlType_network,
            ui_pnlConnect_network,
            NULL
    };
    for (int i = 0; connect_items[i]; i++) lv_obj_clear_flag(connect_items[i], LV_OBJ_FLAG_HIDDEN);

    toggle_static_panels(!config.NETWORK.TYPE);

#define NETWORK(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_network, UDATA);
    NETWORK_ELEMENTS
#undef NETWORK

    overlay_display();
}

static void ui_refresh_task() {
    if (ui_network_locked) {
        exec_watch_task();
        update_network_label();
    }

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, WALL_GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_OK_press();
    ev.code == KEY_ESC && ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txtEntry_network);
}

int muxnetwork_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETWORK.TITLE);
    init_muxnetwork(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_network_values();

    init_osk(ui_pnlEntry_network, ui_txtEntry_network, 1);
    can_scan_check(0);

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
