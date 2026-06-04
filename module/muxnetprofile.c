#include "muxshare.h"
#include "ui/ui_muxnetprofile.h"
#include <sys/wait.h>

#define NETWORK(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(NETWORK_ELEMENTS)
};
#undef NETWORK

#define NET_SCRIPT "script/init/async/S02network.sh"

const char *pass_args[] = {(OPT_PATH "script/web/password.sh"), NULL};
const char *net_c_args[] = {(OPT_PATH NET_SCRIPT), "start", NULL};
const char *net_d_args[] = {(OPT_PATH NET_SCRIPT), "stop", NULL};

#define PASS_ENCODE "********"

#define UI_DHCP (UI_COUNT - 4)
#define UI_STATIC UI_COUNT
#define NET_STATUS_FILE "/run/muos/network.status"
#define IP_OCTET 64

int connecting_phase = 0;
int ui_network_locked = 0;

static char last_status[IP_OCTET] = "";
static char address_file[MAX_BUFFER_SIZE];

static unsigned connect_grace_ticks = 0;

static int fields_modified = 0;
static int network_saved = 0;
static int viewing_existing_profile = 0;
static int viewing_active_profile = 0;

static mux_dialogue save_dlg;
static int save_dlg_active = 0;

static mux_dialogue forget_dlg;
static int forget_dlg_active = 0;

static void list_nav_move(int steps, int direction);

static void save_profile_ini(void);

static void check_focus(void);

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

static void sanitise_ssid_name(char *dest, const char *src) {
    size_t j = 0;
    while (*src && j < MAX_BUFFER_SIZE - 1) {
        dest[j++] = (*src == '/' || *src == '\\') ? '_' : *src;
        src++;
    }
    dest[j] = '\0';
}

static void show_help(void) {
    struct help_msg help_messages[] = {
#define NETWORK(NAME, ENUM, UDATA) { UDATA, lang.MUXNETPROFILE.HELP.ENUM },
            NETWORK_ELEMENTS
#undef NETWORK
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static inline void set_connect_value(const char *value) {
    if (!ui_lblConnectValue_network || !value) return;

    const char *curr = lv_label_get_text(ui_lblConnectValue_network);
    if (!curr || strcmp(curr, value) != 0) lv_label_set_text(ui_lblConnectValue_network, value);
}

static inline void nav_scan_hide(int hide) {
    if (hide) {
        lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static int read_ip(char *buf) {
    FILE *f = fopen(address_file, "r");

    if (f) {
        int ok = (fgets(buf, IP_OCTET, f) != NULL);
        fclose(f);
        if (ok) {
            char *p = buf;
            while (*p && *p != '\n' && *p != '\r') p++;
            *p = '\0';
            if (*buf && strcasecmp(buf, "0.0.0.0") != 0) return 1;
        }
    }

    char cmd[MAX_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "ip addr show %s 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1", device.NETWORK.INTERFACE);
    FILE *pipe = popen(cmd, "r");

    if (!pipe) return 0;

    int ok = (fgets(buf, IP_OCTET, pipe) != NULL);
    pclose(pipe);
    if (!ok) return 0;

    char *p = buf;
    while (*p && *p != '\n' && *p != '\r') p++;
    *p = '\0';

    return *buf != '\0';
}

static const char *net_status_label(int id) {
    switch (id) {
        case STATUS_ASSOCIATING:
            return lang.MUXNETPROFILE.STATUS.ASSOCIATING;
        case STATUS_AUTHENTICATING:
            return lang.MUXNETPROFILE.STATUS.AUTHENTICATING;
        case STATUS_WAITING_IP:
            return lang.MUXNETPROFILE.STATUS.WAITING_IP;
        case STATUS_VALIDATING:
            return lang.MUXNETPROFILE.STATUS.VALIDATING;
        case STATUS_FAILED:
            return lang.MUXNETPROFILE.NOT_CONNECTED;
        case STATUS_INVALID_PASSWORD:
            return lang.MUXNETPROFILE.STATUS.INVALID_PASSWORD;
        case STATUS_AP_NOT_FOUND:
            return lang.MUXNETPROFILE.STATUS.AP_NOT_FOUND;
        case STATUS_AUTH_TIMEOUT:
            return lang.MUXNETPROFILE.STATUS.AUTH_TIMEOUT;
        case STATUS_DHCP_FAILED:
            return lang.MUXNETPROFILE.STATUS.DHCP_FAILED;
        case STATUS_LINK_TIMEOUT:
            return lang.MUXNETPROFILE.STATUS.LINK_TIMEOUT;
        case STATUS_WPA_START_FAILED:
            return lang.MUXNETPROFILE.STATUS.WPA_START_FAILED;
        default:
            return lang.GENERIC.UNKNOWN;
    }
}

static int resolve_status_id(const char *s) {
    if (!strcmp(s, "ASSOCIATING")) return STATUS_ASSOCIATING;
    if (!strcmp(s, "AUTHENTICATING")) return STATUS_AUTHENTICATING;
    if (!strcmp(s, "WAITING_IP")) return STATUS_WAITING_IP;
    if (!strcmp(s, "VALIDATING")) return STATUS_VALIDATING;
    if (!strcmp(s, "CONNECTED")) return STATUS_CONNECTED;
    if (!strcmp(s, "FAILED")) return STATUS_FAILED;
    if (!strcmp(s, "INVALID_PASSWORD")) return STATUS_INVALID_PASSWORD;
    if (!strcmp(s, "AP_NOT_FOUND")) return STATUS_AP_NOT_FOUND;
    if (!strcmp(s, "AUTH_TIMEOUT")) return STATUS_AUTH_TIMEOUT;
    if (!strcmp(s, "DHCP_FAILED")) return STATUS_DHCP_FAILED;
    if (!strcmp(s, "LINK_TIMEOUT")) return STATUS_LINK_TIMEOUT;
    if (!strcmp(s, "WPA_START_FAILED")) return STATUS_WPA_START_FAILED;

    return -1;
}

static void can_scan_check(int forced_disconnect) {
    if (ui_network_locked && connecting_phase) {
        lv_label_set_text(ui_lblConnect_network, lang.MUXNETPROFILE.DISCONNECT);
        nav_scan_hide(1);
        update_network_status(ui_staNetwork, &theme, 0);
        return;
    }

    if (!forced_disconnect && is_network_connected()) {
        lv_label_set_text(ui_lblConnect_network, lang.MUXNETPROFILE.DISCONNECT);
        nav_scan_hide(1);
        update_network_status(ui_staNetwork, &theme, 0);
        return;
    }

    lv_label_set_text(ui_lblConnect_network, lang.MUXNETPROFILE.CONNECT);
    nav_scan_hide(0);

    set_connect_value(lang.MUXNETPROFILE.NOT_CONNECTED);
    update_network_status(ui_staNetwork, &theme, 2);
}

static void get_current_ip(void) {
    char ip[IP_OCTET];

    if (!read_ip(ip)) {
        if (ui_network_locked && connecting_phase) {
            set_connect_value(lang.MUXNETPROFILE.CONNECT_TRY);
            return;
        }

        can_scan_check(1);
        return;
    }

    if (!*ip || !strcasecmp(ip, "0.0.0.0")) {
        if (ui_network_locked && connecting_phase) {
            set_connect_value(lang.MUXNETPROFILE.CONNECT_TRY);
            return;
        }

        can_scan_check(1);
        return;
    }

    connecting_phase = 0;

    set_connect_value(config.NETWORK.TYPE ? lang.MUXNETPROFILE.CONNECTED : ip);
    lv_label_set_text(ui_lblConnect_network, lang.MUXNETPROFILE.DISCONNECT);

    nav_scan_hide(1);
    update_network_status(ui_staNetwork, &theme, 1);
}

static void update_network_label(void) {
    if (!ui_network_locked) return;

    char *status = read_line_char_from(NET_STATUS_FILE, 1);
    if (!status || !*status) {
        if (connecting_phase && connect_grace_ticks < 20) {
            connect_grace_ticks++;
            set_connect_value(lang.MUXNETPROFILE.CONNECT_TRY);
            return;
        }

        get_current_ip();
        return;
    }

    char *p = status;
    while (*p && *p != '\n' && *p != '\r') p++;
    *p = '\0';

    if (strcmp(status, last_status) != 0) {
        snprintf(last_status, sizeof(last_status), "%s", status);

        int id = resolve_status_id(status);

        if (id == STATUS_CONNECTED) {
            connecting_phase = 0;
            ui_network_locked = 0;
            connect_grace_ticks = 0;
            get_current_ip();
            return;
        }

        if (id == STATUS_FAILED || id >= STATUS_INVALID_PASSWORD) {
            connecting_phase = 0;
            ui_network_locked = 0;
            connect_grace_ticks = 0;
        }

        if (id >= 0) set_connect_value(net_status_label(id));
    }
}

static void net_connect_check() {
    ui_network_locked = 0;
    connecting_phase = 0;
    connect_grace_ticks = 0;

    last_status[0] = '\0';
    get_current_ip();
}

static void restore_network_values(void) {
    char *profile_name_raw = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
    viewing_existing_profile = (profile_name_raw && *profile_name_raw);

    if (viewing_existing_profile) {
        char profile_name_buf[MAX_BUFFER_SIZE];
        snprintf(profile_name_buf, sizeof(profile_name_buf), "%s", profile_name_raw);

        char profile_file[MAX_BUFFER_SIZE];
        snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name_buf);

        mini_t *net = mini_try_load(profile_file);
        int is_static = (strcasecmp(mini_get_string(net, "network", "type", "dhcp"), "static") == 0);

        const char *ssid = mini_get_string(net, "network", "ssid", "");
        const char *pass = mini_get_string(net, "network", "pass", "");

        int priority = (int) mini_get_int(net, "network", "priority", 5);

        if (priority < 0) priority = 0;
        if (priority > 9) priority = 9;

        char priority_buf[4];
        snprintf(priority_buf, sizeof(priority_buf), "%d", priority);

        lv_label_set_text(ui_lblProfileNameValue_network, profile_name_buf);
        lv_label_set_text(ui_lblTypeValue_network, is_static ? lang.MUXNETPROFILE.STATIC : lang.MUXNETPROFILE.DHCP);
        lv_label_set_text(ui_lblPriorityValue_network, priority_buf);
        ui_count = is_static ? UI_STATIC : UI_DHCP;
        lv_label_set_text(ui_lblIdentifierValue_network, ssid);
        lv_label_set_text(ui_lblPasswordValue_network, strlen(pass) >= IP_OCTET ? PASS_ENCODE : pass);
        lv_label_set_text(ui_lblAddressValue_network, mini_get_string(net, "network", "address", ""));
        lv_label_set_text(ui_lblSubnetValue_network, mini_get_string(net, "network", "subnet", ""));
        lv_label_set_text(ui_lblGatewayValue_network, mini_get_string(net, "network", "gateway", ""));
        lv_label_set_text(ui_lblDnsValue_network, mini_get_string(net, "network", "dns", ""));
        mini_free(net);
    } else {
        char ssid_buf[MAX_BUFFER_SIZE];
        char *pending_ssid = read_line_char_from(CONF_CONFIG_PATH "network/ssid", 1);
        snprintf(ssid_buf, sizeof(ssid_buf), "%s", (pending_ssid && *pending_ssid) ? pending_ssid : "");

        lv_label_set_text(ui_lblProfileNameValue_network, ssid_buf);
        lv_label_set_text(ui_lblTypeValue_network, lang.MUXNETPROFILE.DHCP);
        lv_label_set_text(ui_lblPriorityValue_network, "5");
        ui_count = UI_DHCP;
        lv_label_set_text(ui_lblIdentifierValue_network, ssid_buf);
        lv_label_set_text(ui_lblPasswordValue_network, "");
        lv_label_set_text(ui_lblAddressValue_network, "");
        lv_label_set_text(ui_lblSubnetValue_network, "");
        lv_label_set_text(ui_lblGatewayValue_network, "");
        lv_label_set_text(ui_lblDnsValue_network, "");
    }

    const char *profile_ssid = lv_label_get_text(ui_lblIdentifierValue_network);
    const char *active_ssid = config.NETWORK.SSID;

    viewing_active_profile = 0;

    if (profile_ssid && *profile_ssid && is_network_connected()) {
        if (active_ssid && *active_ssid) viewing_active_profile = (strcmp(profile_ssid, active_ssid) == 0);

        if (!viewing_active_profile) {
            char cmd[MAX_BUFFER_SIZE];
            snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null | awk -F': ' '/SSID/ {print $2}'", device.NETWORK.INTERFACE);
            FILE *iw_pipe = popen(cmd, "r");

            if (iw_pipe) {
                char live_ssid[MAX_BUFFER_SIZE] = {0};
                if (fgets(live_ssid, sizeof(live_ssid), iw_pipe)) {
                    char *p = live_ssid;
                    while (*p && *p != '\n' && *p != '\r') p++;
                    *p = '\0';
                    viewing_active_profile = (*live_ssid && strcmp(profile_ssid, live_ssid) == 0);
                }

                pclose(iw_pipe);
            }
        }
    }
}

static void escape_wpa_string(const char *src, char *dst) {
    size_t di = 0;

    for (const char *p = src; *p && di + 2 < MAX_BUFFER_SIZE; p++) {
        if (*p == '"' || *p == '\\') dst[di++] = '\\';
        dst[di++] = *p;
    }

    dst[di] = '\0';
}

static void save_network_config(void) {
    int idx_type = 0;

    if (strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETPROFILE.STATIC) == 0) idx_type = 1;

    char esc_ssid[MAX_BUFFER_SIZE];
    escape_wpa_string(lv_label_get_text(ui_lblIdentifierValue_network), esc_ssid);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/type", INT, idx_type);
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid", CHAR, lv_label_get_text(ui_lblIdentifierValue_network));
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid_wpa", CHAR, esc_ssid);

    if (strcasecmp(lv_label_get_text(ui_lblPasswordValue_network), PASS_ENCODE) != 0) {
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/pass", CHAR, lv_label_get_text(ui_lblPasswordValue_network));
    }

    if (idx_type) {
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/address", CHAR, lv_label_get_text(ui_lblAddressValue_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/subnet", CHAR, lv_label_get_text(ui_lblSubnetValue_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/gateway", CHAR, lv_label_get_text(ui_lblGatewayValue_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/dns", CHAR, lv_label_get_text(ui_lblDnsValue_network));
    }

    refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, network, ProfileName, lang.MUXNETPROFILE.PROFILE_NAME, "profile_name", "");
    INIT_VALUE_ITEM(-1, network, Identifier, lang.MUXNETPROFILE.IDENTIFIER, "identifier", "");
    INIT_VALUE_ITEM(-1, network, Password, lang.MUXNETPROFILE.PASSWORD, "password", "");
    INIT_VALUE_ITEM(-1, network, Type, lang.MUXNETPROFILE.TYPE, "type", "");
    INIT_VALUE_ITEM(-1, network, Priority, lang.MUXNETPROFILE.PRIORITY, "priority", "");
    INIT_VALUE_ITEM(-1, network, Address, lang.MUXNETPROFILE.ADDRESS, "address", "");
    INIT_VALUE_ITEM(-1, network, Subnet, lang.MUXNETPROFILE.SUBNET, "subnet", "");
    INIT_VALUE_ITEM(-1, network, Gateway, lang.MUXNETPROFILE.GATEWAY, "gateway", "");
    INIT_VALUE_ITEM(-1, network, Dns, lang.MUXNETPROFILE.DNS, "dns", "");
    INIT_VALUE_ITEM(-1, network, Connect, lang.MUXNETPROFILE.CONNECT, "connect", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(0, +1, 1, 0);
    nav_moved = 1;

    check_focus();
}

static void check_focus() {
    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    int show_lr = !viewing_active_profile && ((e_focused == ui_lblPriority_network) || (e_focused == ui_lblType_network));

    if (show_lr) {
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, 0, 0);
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
    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lblProfileName_network) {
        const char *new_profile = lv_textarea_get_text(ui_txtEntry_network);
        if (!new_profile || !*new_profile) new_profile = lv_label_get_text(ui_lblIdentifierValue_network);
        lv_label_set_text(ui_lblProfileNameValue_network, new_profile);
    } else if (e_focused == ui_lblIdentifier_network) {
        const char *old_ssid = lv_label_get_text(ui_lblIdentifierValue_network);
        const char *cur_profile = lv_label_get_text(ui_lblProfileNameValue_network);
        const char *new_ssid = lv_textarea_get_text(ui_txtEntry_network);

        if (!cur_profile || !*cur_profile || strcmp(cur_profile, old_ssid) == 0) lv_label_set_text(ui_lblProfileNameValue_network, new_ssid);
        lv_label_set_text(ui_lblIdentifierValue_network, new_ssid);
    } else if (e_focused == ui_lblPassword_network) {
        lv_label_set_text(ui_lblPasswordValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (e_focused == ui_lblAddress_network) {
        lv_label_set_text(ui_lblAddressValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (e_focused == ui_lblSubnet_network) {
        lv_label_set_text(ui_lblSubnetValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (e_focused == ui_lblGateway_network) {
        lv_label_set_text(ui_lblGatewayValue_network, lv_textarea_get_text(ui_txtEntry_network));
    } else if (e_focused == ui_lblDns_network) {
        lv_label_set_text(ui_lblDnsValue_network, lv_textarea_get_text(ui_txtEntry_network));
    }

    if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
        reset_osk(num_entry);
    } else {
        reset_osk(key_entry);
    }

    lv_textarea_set_text(ui_txtEntry_network, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnlEntry_network);
    fields_modified = 1;
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_KEYPRESS);

    lv_obj_t *active = lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN) ? num_entry : key_entry;
    const char *is_key = lv_btnmatrix_get_btn_text(active, key_curr);

    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else {
        lv_event_send(active, LV_EVENT_CLICKED, &key_curr);
    }
}

static void toggle_static_panels(int hide) {
    struct {
        lv_obj_t *panel;
        lv_obj_t *label;
    } items[] = {
            {ui_pnlAddress_network, ui_lblAddress_network},
            {ui_pnlSubnet_network,  ui_lblSubnet_network},
            {ui_pnlGateway_network, ui_lblGateway_network},
            {ui_pnlDns_network,     ui_lblDns_network},
            {NULL, NULL}
    };

    for (int i = 0; items[i].panel; i++) {
        if (hide) {
            lv_obj_add_flag(items[i].panel, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_add_flag(items[i].label, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_clear_flag(items[i].panel, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(items[i].label, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

static void handle_priority_change(int delta) {
    if (ui_network_locked) return;

    if (viewing_active_profile) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNETPROFILE.DENY_MODIFY, SHORT);
        return;
    }

    int next = safe_atoi(lv_label_get_text(ui_lblPriorityValue_network), 0) + delta;
    if (next < 0) next = 9;
    if (next > 9) next = 0;

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", next);
    lv_label_set_text(ui_lblPriorityValue_network, buf);

    play_sound(SND_OPTION);
    fields_modified = 1;
}

int handle_navigate(void) {
    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lblPriority_network) return 1;

    if (e_focused == ui_lblType_network) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETPROFILE.DENY_MODIFY, SHORT);

            return 1;
        }

        play_sound(SND_OPTION);

        int is_static = strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETPROFILE.STATIC) == 0;
        lv_label_set_text(ui_lblTypeValue_network, is_static ? lang.MUXNETPROFILE.DHCP : lang.MUXNETPROFILE.STATIC);
        ui_count = is_static ? UI_DHCP : UI_STATIC;
        toggle_static_panels(is_static);

        return 1;
    }

    return 0;
}

static void handle_confirm(void) {
    if (handle_navigate()) return;

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lblConnect_network) {
        if (lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM);
            write_text_to_file_atomic(address_file, CHAR, "");
            run_exec(net_d_args, A_SIZE(net_d_args), 0, 0, NULL, NULL);
            can_scan_check(1);
        } else {
            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_lblIdentifierValue_network);

            char password_buf[MAX_BUFFER_SIZE];
            snprintf(password_buf, sizeof(password_buf), "%s", lv_label_get_text(ui_lblPasswordValue_network));
            size_t cv_pass_len = strlen(password_buf);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password
            int cv_pass_ok = (cv_pass_len == 0 || (cv_pass_len >= 8 && cv_pass_len <= 63));

            if (strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETPROFILE.STATIC) == 0) {
                const char *cv_address = lv_label_get_text(ui_lblAddressValue_network);
                const char *cv_subnet = lv_label_get_text(ui_lblSubnetValue_network);
                const char *cv_gateway = lv_label_get_text(ui_lblGatewayValue_network);
                const char *cv_dns = lv_label_get_text(ui_lblDnsValue_network);

                if (strlen(cv_ssid) > 0 && cv_pass_ok && strlen(cv_address) > 0 &&
                    strlen(cv_subnet) > 0 && strlen(cv_gateway) > 0 && strlen(cv_dns) > 0) {
                    valid_info = 1;
                }
            } else {
                if (strlen(cv_ssid) > 0 && cv_pass_ok) valid_info = 1;
            }

            if (valid_info) {
                play_sound(SND_CONFIRM);
                save_network_config();
                save_profile_ini();
                network_saved = 1;

                if (cv_pass_len > 0) {
                    if (strcasecmp(password_buf, PASS_ENCODE) != 0 && strcasecmp(password_buf, "") != 0) {
                        lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETPROFILE.ENCRYPT_PASSWORD);
                    }
                } else {
                    lv_label_set_text(ui_lblConnectValue_network, lang.MUXNETPROFILE.NO_PASSWORD);
                }

                lv_label_set_text(ui_lblPasswordValue_network, PASS_ENCODE);
                set_connect_value(lang.MUXNETPROFILE.CONNECT_TRY);
                lv_task_handler();

                connecting_phase = 1;
                connect_grace_ticks = 0;

                ui_network_locked = 1;
                run_exec(pass_args, A_SIZE(pass_args), 0, 0, NULL, NULL);

                memset(password_buf, 0, sizeof(password_buf));
                lv_textarea_set_text(ui_txtEntry_network, "");

                lv_task_handler();

                last_status[0] = '\0';
                run_exec(net_c_args, A_SIZE(net_c_args), 1, 0, NULL, net_connect_check);
                lv_task_handler();
            } else {
                play_sound(SND_ERROR);
                toast_message(lang.MUXNETPROFILE.CHECK, SHORT);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(SND_CONFIRM);
            key_curr = 0;
            if (e_focused == ui_lblProfileName_network || e_focused == ui_lblIdentifier_network || e_focused == ui_lblPassword_network) {
                lv_textarea_set_password_mode(ui_txtEntry_network, e_focused == ui_lblPassword_network);

                lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

                lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_state(num_entry, LV_STATE_DISABLED);

                key_show = 1;
            } else {
                lv_textarea_set_password_mode(ui_txtEntry_network, 0);

                lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(num_entry, LV_STATE_DISABLED);

                lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_state(key_entry, LV_STATE_DISABLED);

                key_show = 2;
            }

            osk_show(ui_pnlEntry_network);
            osk_refresh_labels();

            lv_textarea_set_text(ui_txtEntry_network, e_focused == ui_lblPassword_network ? "" : lv_label_get_text(lv_group_get_focused(ui_group_value)));
        } else {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNETPROFILE.DENY_MODIFY, SHORT);
        }
    }
}

static void handle_back(void) {
    if (fields_modified && !network_saved) {
        play_sound(SND_CONFIRM);

        save_dlg_active = 1;
        save_dlg.selected = 0;

        dialogue_show(&save_dlg);
        dialogue_refresh(&save_dlg, &theme);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_profile");

    mux_input_stop();
}

static void handle_scan(void) {
    if (ui_network_locked) return;

    if (!lv_obj_has_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN)) {
        play_sound(SND_CONFIRM);

        save_network_config();
        load_mux("net_scan");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        mux_input_stop();
    }
}

static void do_forget_profile(void) {
    char *profile_name_raw = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
    if (!profile_name_raw || !*profile_name_raw) return;

    char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name_raw);

    remove(profile_file);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, "");
    viewing_existing_profile = 0;
}

static int derive_psk(const char *ssid, const char *pass, char *out) {
    if (!ssid || !*pass || strlen(pass) == 64) return 0;

    int pfd[2];
    if (pipe(pfd) != 0) return 0;

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return 0;
    }

    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);

        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) {
            dup2(dn, STDERR_FILENO);
            close(dn);
        }

        const char *args[] = {"wpa_passphrase", ssid, pass, NULL};
        execvp("wpa_passphrase", (char *const *) args);
        _exit(1);
    }

    close(pfd[1]);

    char buf[4096] = {0};
    ssize_t n, total = 0;
    while (total < (ssize_t) (sizeof(buf) - 1) && (n = read(pfd[0], buf + total, sizeof(buf) - 1 - total)) > 0) {
        total += n;
    }

    close(pfd[0]);

    int ws;
    waitpid(pid, &ws, 0);
    if (!WIFEXITED(ws) || WEXITSTATUS(ws) != 0) return 0;

    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "psk=", 4) == 0) {
            const char *psk = p + 4;
            char *eol = strchr(psk, '\n');
            size_t psk_len = eol ? (size_t) (eol - psk) : strlen(psk);
            if (psk_len == 64) {
                snprintf(out, 65, "%.*s", (int) psk_len, psk);
                return 1;
            }
        }
        char *eol = strchr(p, '\n');
        if (!eol) break;
        p = eol + 1;
    }

    return 0;
}

static void save_profile_ini(void) {
    const char *ssid = lv_label_get_text(ui_lblIdentifierValue_network);
    if (!ssid || !*ssid) return;

    const char *profile_name_raw = lv_label_get_text(ui_lblProfileNameValue_network);
    char profile_name[MAX_BUFFER_SIZE];

    if (profile_name_raw && *profile_name_raw) {
        sanitise_ssid_name(profile_name, profile_name_raw);
    } else {
        sanitise_ssid_name(profile_name, ssid);
    }

    if (!*profile_name) return;

    mkdir(STORAGE_NETWORK, 0755);

    if (viewing_existing_profile) {
        char *old_name = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
        if (old_name && *old_name && strcmp(old_name, profile_name) != 0) {
            char old_file[MAX_BUFFER_SIZE];
            snprintf(old_file, sizeof(old_file), STORAGE_NETWORK "/%s.ini", old_name);
            remove(old_file);
        }
    }

    char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);

    int is_static = (strcasecmp(lv_label_get_text(ui_lblTypeValue_network), lang.MUXNETPROFILE.STATIC) == 0);

    mini_t *net = mini_try_load(profile_file);

    char pass_buf[MAX_BUFFER_SIZE];
    const char *pass_label = lv_label_get_text(ui_lblPasswordValue_network);
    if (strcasecmp(pass_label, PASS_ENCODE) == 0) {
        snprintf(pass_buf, sizeof(pass_buf), "%s", mini_get_string(net, "network", "pass", ""));
    } else {
        snprintf(pass_buf, sizeof(pass_buf), "%s", pass_label ? pass_label : "");
    }

    if (*pass_buf && strlen(pass_buf) != 64) {
        char psk[65] = {0};
        if (derive_psk(ssid, pass_buf, psk)) snprintf(pass_buf, sizeof(pass_buf), "%s", psk);
    }

    mini_set_string(net, "network", "ssid", ssid);
    mini_set_string(net, "network", "pass", pass_buf);
    mini_set_string(net, "network", "type", is_static ? "static" : "dhcp");

    if (is_static) {
        mini_set_string(net, "network", "address", lv_label_get_text(ui_lblAddressValue_network));
        mini_set_string(net, "network", "subnet", lv_label_get_text(ui_lblSubnetValue_network));
        mini_set_string(net, "network", "gateway", lv_label_get_text(ui_lblGatewayValue_network));
        mini_set_string(net, "network", "dns", lv_label_get_text(ui_lblDnsValue_network));
    }

    mini_set_int(net, "network", "autoconnect", (int) mini_get_int(net, "network", "autoconnect", 1));

    int pri = safe_atoi(lv_label_get_text(ui_lblPriorityValue_network), 0);
    if (pri < 0) pri = 0;
    if (pri > 9) pri = 9;

    mini_set_int(net, "network", "priority", pri);
    mini_save(net, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, profile_name);
    viewing_existing_profile = 1;

    lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void handle_a(void) {
    if (forget_dlg_active) {
        mux_confirm_opt opt = (mux_confirm_opt) forget_dlg.selected;
        forget_dlg_active = 0;
        dialogue_hide(&forget_dlg);

        if (opt == MUX_CONFIRM_YEP) {
            do_forget_profile();

            play_sound(SND_CONFIRM);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_profile");

            mux_input_stop();
        }

        return;
    }

    if (save_dlg_active) {
        mux_confirm_opt opt = (mux_confirm_opt) save_dlg.selected;
        save_dlg_active = 0;
        dialogue_hide(&save_dlg);

        if (opt == MUX_CONFIRM_YEP) {
            if (viewing_active_profile) save_network_config();
            save_profile_ini();
        }

        play_sound(SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_profile");

        mux_input_stop();

        return;
    }

    if (msgbox_active || hold_call || ui_network_locked) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_b(void) {
    if (hold_call) return;

    if (forget_dlg_active) {
        forget_dlg_active = 0;
        dialogue_hide(&forget_dlg);

        play_sound(SND_BACK);
        return;
    }

    if (save_dlg_active) {
        save_dlg_active = 0;
        dialogue_hide(&save_dlg);

        play_sound(SND_BACK);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txtEntry_network);
        return;
    }

    handle_back();
}

static void handle_b_hold(void) {
    if (forget_dlg_active || save_dlg_active) return;

    if (key_show) key_backspace(ui_txtEntry_network);
}

static void handle_x(void) {
    if (forget_dlg_active || msgbox_active || hold_call || ui_network_locked) return;

    if (key_show) {
        close_osk(lv_obj_has_state(key_entry, LV_STATE_DISABLED) ? num_entry : key_entry, ui_group, ui_txtEntry_network, ui_pnlEntry_network);
        return;
    }

    handle_scan();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || ui_network_locked) return;

    if (key_show == 1) {
        key_space(ui_txtEntry_network);
        return;
    }

    if (key_show == 2 || !viewing_existing_profile) return;

    if (viewing_active_profile) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNETPROFILE.DENY_FORGET, SHORT);
        return;
    }

    play_sound(SND_INFO_OPEN);

    forget_dlg_active = 1;
    forget_dlg.selected = 0;

    dialogue_show(&forget_dlg);
    dialogue_refresh(&forget_dlg, &theme);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call || ui_network_locked) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void handle_up(void) {
    if (forget_dlg_active) {
        if (!swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (save_dlg_active) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    key_show ? key_up() : handle_list_nav_up();
}

static void handle_up_hold(void) {
    if (forget_dlg_active || save_dlg_active) return;

    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
    if (forget_dlg_active) {
        if (!swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (save_dlg_active) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    key_show ? key_down() : handle_list_nav_down();
}

static void handle_down_hold(void) {
    if (forget_dlg_active || save_dlg_active) return;

    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (forget_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (save_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lblPriority_network) {
        handle_priority_change(-1);
        return;
    }

    key_show ? key_left() : handle_navigate();
}

static void handle_right(void) {
    if (forget_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&forget_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (save_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lblPriority_network) {
        handle_priority_change(+1);
        return;
    }

    key_show ? key_right() : handle_navigate();
}

static void handle_left_hold(void) {
    if (forget_dlg_active || save_dlg_active || ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lblPriority_network) {
        handle_priority_change(-1);
        return;
    }

    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (forget_dlg_active || save_dlg_active || ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lblPriority_network) {
        handle_priority_change(+1);
        return;
    }

    if (key_show) key_right();
}

static void handle_l1(void) {
    if (key_show == 1) {
        key_swap_back();
        return;
    }

    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show == 1) {
        key_swap();
        return;
    }

    if (!key_show) handle_list_nav_page_down();
}

static void check_connecting_state(void) {
    FILE *f = fopen(NET_STATUS_FILE, "r");
    if (!f) return;

    char status[64] = {0};
    int ok = (fgets(status, sizeof(status), f) != NULL);
    fclose(f);

    if (!ok) return;

    char *p = status;
    while (*p && *p != '\n' && *p != '\r') p++;
    *p = '\0';

    int id = resolve_status_id(status);
    if (id == STATUS_ASSOCIATING || id == STATUS_AUTHENTICATING || id == STATUS_WAITING_IP || id == STATUS_VALIDATING) {
        ui_network_locked = 1;
        connecting_phase = 1;
    }
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
            {ui_lblNavLRGlyph, "",                        0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE,       0},
            {ui_lblNavBGlyph,  "",                        0},
            {ui_lblNavB,       lang.GENERIC.BACK,         0},
            {ui_lblNavXGlyph,  "",                        0},
            {ui_lblNavX,       lang.GENERIC.SCAN,         0},
            {ui_lblNavYGlyph,  "",                        0},
            {ui_lblNavY,       lang.MUXNETPROFILE.FORGET, 0},
            {NULL, NULL,                                  0}
    });

    lv_obj_t *connect_items[] = {
            ui_pnlProfileName_network,
            ui_pnlIdentifier_network,
            ui_pnlPassword_network,
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

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_OK_press();
    ev.code == KEY_ESC && ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txtEntry_network);
}

int muxnetprofile_main(void) {
    fields_modified = 0;
    network_saved = 0;
    save_dlg_active = 0;
    forget_dlg_active = 0;
    connecting_phase = 0;
    ui_network_locked = 0;
    connect_grace_ticks = 0;
    viewing_active_profile = 0;

    last_status[0] = '\0';

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETPROFILE.TITLE);
    init_muxnetwork(ui_screen, ui_pnlContent, &theme);
    init_elements();

    snprintf(address_file, sizeof(address_file), CONF_CONFIG_PATH "/network/address");

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_network_values();
    check_connecting_state();

    if (!viewing_existing_profile) {
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    init_osk(ui_pnlEntry_network, ui_txtEntry_network, 1, 1, OSK_MAX);
    can_scan_check(viewing_active_profile ? 0 : 1);

    if (viewing_active_profile) {
        if (config.NETWORK.TYPE) {
            set_connect_value(lang.MUXNETPROFILE.CONNECTED);
        } else {
            char ip[IP_OCTET] = {0};
            if (read_ip(ip) && *ip && strcasecmp(ip, "0.0.0.0") != 0) {
                set_connect_value(ip);
            }
        }
    }

    dialogue_init_confirm(&save_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);

    dialogue_init_confirm(&forget_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, lang.MUXNETPROFILE.FORGET_CONFIRM,
                          lang.MUXNETPROFILE.FORGET, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_B] = handle_b_hold,
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
    init_input(&input_opts, 1);

    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
