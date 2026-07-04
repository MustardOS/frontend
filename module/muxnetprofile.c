#include "muxshare.h"
#include "ui/ui_muxnetprofile.h"
#include <sys/wait.h>

#define NETWORK(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(NETWORK_ELEMENTS) };
#undef NETWORK

#define NET_SCRIPT "script/init/async/S02network.sh"

const char *pass_args[] = {OPT_PATH "script/web/password.sh", NULL};
const char *net_c_args[] = {OPT_PATH NET_SCRIPT, "connect", NULL};
const char *net_d_args[] = {OPT_PATH NET_SCRIPT, "stop", NULL};

#define PASS_ENCODE "********"

#define CONNECT_SETTLE_TICKS 120

#define UI_DHCP        (ui_count_dynamic - 4)
#define UI_STATIC      ui_count_dynamic
#define NET_STATUS_DIR "/run/muos/network"
#define IP_OCTET       64

int connecting_phase = 0;
int ui_network_locked = 0;

static char last_status[IP_OCTET] = "";
static char address_file[MAX_BUFFER_SIZE];
static char current_profile[MAX_BUFFER_SIZE] = "";
static char pending_password[MAX_BUFFER_SIZE] = "";

static unsigned connect_grace_ticks = 0;

static int connect_process_done = 1;
static int connect_exit_code = 0;
static unsigned connect_settle_ticks = 0;

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
    status_associating,
    status_authenticating,
    status_waiting_ip,
    status_validating,
    status_connected,
    status_failed,

    // These are worse! Try not to get these...
    status_invalid_password,
    status_ap_not_found,
    status_auth_timeout,
    status_dhcp_failed,
    status_link_timeout,
    status_wpa_start_failed
};

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define NETWORK(NAME, UDATA) {UDATA, lang.muxnetprofile.help.NAME},
        NETWORK_ELEMENTS
#undef NETWORK
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void profile_status_path(char *buf) {
    snprintf(buf, MAX_BUFFER_SIZE, NET_STATUS_DIR "/%s.status", *current_profile ? current_profile : "_none_");
}

static void net_trim(char *value) {
    const char *start = value;
    while (*start && isspace((unsigned char) *start))
        start++;

    if (start != value) memmove(value, start, strlen(start) + 1);

    size_t len = strlen(value);
    while (len > 0 && isspace((unsigned char) value[len - 1])) {
        value[--len] = '\0';
    }
}

static int read_wpa_status_value(const char *key, char *value) {
    if (!*key) return 0;

    value[0] = '\0';

    FILE *fp = popen("wpa_cli status 2>/dev/null", "r");
    if (!fp) return 0;

    char line[MAX_BUFFER_SIZE];
    const size_t key_len = strlen(key);
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        net_trim(line);

        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            snprintf(value, MAX_BUFFER_SIZE, "%s", line + key_len + 1);
            net_trim(value);
            found = *value;
            break;
        }
    }

    pclose(fp);
    return found;
}

static int read_connected_ssid(char *ssid) {
    ssid[0] = '\0';

    char state[MAX_BUFFER_SIZE];
    if (!read_wpa_status_value("wpa_state", state)) return 0;
    if (strcmp(state, "COMPLETED") != 0) return 0;

    return read_wpa_status_value("ssid", ssid);
}

static int current_profile_matches_ssid(const char *ssid) {
    if (!*current_profile || !*ssid) return 0;

    if (strcmp(current_profile, ssid) == 0) return 1;

    char profile_file[MAX_BUFFER_SIZE];
    const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", current_profile);
    if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) return 0;

    mini_t *net = mini_try_load(profile_file);
    const char *profile_ssid = mini_get_string(net, "network", "ssid", "");
    const int match = profile_ssid && *profile_ssid && strcmp(profile_ssid, ssid) == 0;
    mini_free(net);

    return match;
}

static void clear_current_active_profile(void) {
    char *active = read_line_char_from(CONF_CONFIG_PATH "network/active", 1);
    const int match = active && *active && *current_profile && strcmp(active, current_profile) == 0;
    free(active);

    if (match) write_text_to_file_atomic(CONF_CONFIG_PATH "network/active", CHAR, "");
}

static int profile_is_active(void) {
    if (!*current_profile) return 0;

    char state[MAX_BUFFER_SIZE];
    if (read_wpa_status_value("wpa_state", state)) {
        if (strcmp(state, "COMPLETED") == 0) {
            char ssid[MAX_BUFFER_SIZE];
            return read_wpa_status_value("ssid", ssid) && current_profile_matches_ssid(ssid);
        }

        return 0;
    }

    char *active = read_line_char_from(CONF_CONFIG_PATH "network/active", 1);
    const int active_match = active && *active && strcmp(active, current_profile) == 0;
    free(active);

    return active_match;
}

static int is_safe_iface(const char *iface) {
    if (!*iface) return 0;

    const size_t len = strlen(iface);
    if (len >= 16) return 0;

    for (const char *p = iface; *p; p++) {
        if (!isalnum((unsigned char) *p) && *p != '_' && *p != '-' && *p != '.' && *p != ':') {
            return 0;
        }
    }

    return 1;
}

static int read_ip(char *buf) {
    FILE *f = fopen(address_file, "r");

    if (f) {
        const int ok = fgets(buf, IP_OCTET, f) != NULL;
        fclose(f);
        if (ok) {
            char *p = buf;
            while (*p && *p != '\n' && *p != '\r')
                p++;
            *p = '\0';
            if (*buf && strcasecmp(buf, "0.0.0.0") != 0) return 1;
        }
    }

    if (!is_safe_iface(device.network.interface)) return 0;

    char cmd[MAX_BUFFER_SIZE];
    snprintf(
        cmd, sizeof(cmd), "ip addr show %s 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1",
        device.network.interface
    );
    FILE *pipe = popen(cmd, "r");

    if (!pipe) return 0;

    const int ok = fgets(buf, IP_OCTET, pipe) != NULL;
    pclose(pipe);
    if (!ok) return 0;

    char *p = buf;
    while (*p && *p != '\n' && *p != '\r')
        p++;
    *p = '\0';

    return *buf != '\0';
}

static int live_network_connected(void) {
    char ssid[MAX_BUFFER_SIZE];
    if (read_connected_ssid(ssid)) return 1;

    char ip[IP_OCTET];
    return read_ip(ip) && *ip && strcasecmp(ip, "0.0.0.0") != 0;
}

static int active_profile_blocks_connect(void) {
    if (!*current_profile) return 0;
    if (profile_is_active()) return 0;

    char *active = read_line_char_from(CONF_CONFIG_PATH "network/active", 1);
    const int blocked = active && *active && strcmp(active, current_profile) != 0 && live_network_connected();

    free(active);

    return blocked;
}

static int sanitise_ssid_name(char *dest, const char *src) {
    size_t j = 0;

    for (; *src && j < MAX_BUFFER_SIZE - 1; src++) {
        const unsigned char c = (unsigned char) *src;
        char out;

        if (isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.' || c == '(' || c == ')') {
            out = (char) c;
        } else {
            out = '_';
        }

        if (out == '_' && j > 0 && dest[j - 1] == '_') continue;

        dest[j++] = out;
    }
    dest[j] = '\0';

    while (j > 0 && (dest[j - 1] == ' ' || dest[j - 1] == '.'))
        dest[--j] = '\0';

    size_t start = 0;
    while (dest[start] == ' ' || dest[start] == '.')
        start++;
    if (start > 0) memmove(dest, dest + start, j - start + 1);

    return dest[0] != '\0';
}

static void set_connect_value(const char *value) {
    if (!ui_val_connect_network || !value) return;

    const char *curr = lv_label_get_text(ui_val_connect_network);
    if (!curr || strcmp(curr, value) != 0) lv_label_set_text(ui_val_connect_network, value);
}

static void nav_scan_hide(const int hide) {
    if (hide) {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static const char *net_status_label(const int id) {
    switch (id) {
        case status_associating:
            return lang.muxnetprofile.status.associating;
        case status_authenticating:
            return lang.muxnetprofile.status.authenticating;
        case status_waiting_ip:
            return lang.muxnetprofile.status.waiting_ip;
        case status_validating:
            return lang.muxnetprofile.status.validating;
        case status_failed:
            return lang.muxnetprofile.not_connected;
        case status_invalid_password:
            return lang.muxnetprofile.status.invalid_password;
        case status_ap_not_found:
            return lang.muxnetprofile.status.ap_not_found;
        case status_auth_timeout:
            return lang.muxnetprofile.status.auth_timeout;
        case status_dhcp_failed:
            return lang.muxnetprofile.status.dhcp_failed;
        case status_link_timeout:
            return lang.muxnetprofile.status.link_timeout;
        case status_wpa_start_failed:
            return lang.muxnetprofile.status.wpa_start_failed;
        default:
            return lang.generic.unknown;
    }
}

static int resolve_status_id(const char *s) {
    if (!strcmp(s, "ASSOCIATING")) return status_associating;
    if (!strcmp(s, "AUTHENTICATING")) return status_authenticating;
    if (!strcmp(s, "WAITING_IP")) return status_waiting_ip;
    if (!strcmp(s, "VALIDATING")) return status_validating;
    if (!strcmp(s, "CONNECTED")) return status_connected;
    if (!strcmp(s, "FAILED")) return status_failed;
    if (!strcmp(s, "INVALID_PASSWORD")) return status_invalid_password;
    if (!strcmp(s, "AP_NOT_FOUND")) return status_ap_not_found;
    if (!strcmp(s, "AUTH_TIMEOUT")) return status_auth_timeout;
    if (!strcmp(s, "DHCP_FAILED")) return status_dhcp_failed;
    if (!strcmp(s, "LINK_TIMEOUT")) return status_link_timeout;
    if (!strcmp(s, "WPA_START_FAILED")) return status_wpa_start_failed;

    return -1;
}

static int status_from_connect_exit_code(const int exit_code) {
    switch (exit_code) {
        case 2:
            return status_invalid_password;
        case 3:
            return status_ap_not_found;
        case 4:
            return status_auth_timeout;
        case 5:
            return status_dhcp_failed;
        case 6:
            return status_link_timeout;
        case 7:
            return status_wpa_start_failed;
        default:
            return status_failed;
    }
}

static void clear_profile_status_file(void) {
    char status_path[MAX_BUFFER_SIZE];
    profile_status_path(status_path);
    remove(status_path);
}

static void show_connect_failure(const int exit_code) {
    const int id = status_from_connect_exit_code(exit_code);

    ui_network_locked = 0;
    connecting_phase = 0;
    connect_grace_ticks = 0;
    connect_settle_ticks = 0;
    viewing_active_profile = 0;

    clear_current_active_profile();

    if (id >= 0) set_connect_value(net_status_label(id));

    lv_label_set_text(ui_lbl_connect_network, lang.muxnetprofile.connect);
    nav_scan_hide(0);
    update_network_status(ui_sta_network, &theme, 2);
}

static void can_scan_check(const int forced_disconnect) {
    if (ui_network_locked && connecting_phase) {
        lv_label_set_text(ui_lbl_connect_network, lang.muxnetprofile.disconnect);
        nav_scan_hide(1);
        update_network_status(ui_sta_network, &theme, 0);
        return;
    }

    if (!forced_disconnect && profile_is_active()) {
        lv_label_set_text(ui_lbl_connect_network, lang.muxnetprofile.disconnect);
        nav_scan_hide(1);
        update_network_status(ui_sta_network, &theme, 0);
        return;
    }

    lv_label_set_text(ui_lbl_connect_network, lang.muxnetprofile.connect);
    nav_scan_hide(0);

    set_connect_value(lang.muxnetprofile.not_connected);
    update_network_status(ui_sta_network, &theme, 2);
}

static int get_current_ip(void) {
    char ip[IP_OCTET];

    const int has_ip = read_ip(ip) && *ip && strcasecmp(ip, "0.0.0.0") != 0;

    if (has_ip && (profile_is_active() || (ui_network_locked && connecting_phase))) {
        connecting_phase = 0;
        viewing_active_profile = 1;
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/active", CHAR, current_profile);

        set_connect_value(ip);
        lv_label_set_text(ui_lbl_connect_network, lang.muxnetprofile.disconnect);

        nav_scan_hide(1);
        update_network_status(ui_sta_network, &theme, 1);

        return 1;
    }

    if (ui_network_locked && connecting_phase) {
        set_connect_value(lang.muxnetprofile.connect_try);
        return 0;
    }

    can_scan_check(1);
    return 0;
}

static void update_network_label(void) {
    if (!ui_network_locked) return;

    char status_path[MAX_BUFFER_SIZE];
    profile_status_path(status_path);

    char *status = read_line_char_from(status_path, 1);

    if (status && *status) {
        char *p = status;
        while (*p && *p != '\n' && *p != '\r')
            p++;
        *p = '\0';

        const int id = resolve_status_id(status);
        const int status_changed = strcmp(status, last_status) != 0;

        if (status_changed) {
            snprintf(last_status, sizeof(last_status), "%s", status);

            if (id >= 0 && id != status_connected) {
                set_connect_value(net_status_label(id));
            }
        }

        if (id == status_connected) {
            if (get_current_ip()) {
                ui_network_locked = 0;
                connecting_phase = 0;
                connect_grace_ticks = 0;
                connect_settle_ticks = 0;
                free(status);
                return;
            }

            connecting_phase = 1;
            set_connect_value(lang.muxnetprofile.connect_try);
            free(status);
            return;
        }

        if (id == status_failed || id >= status_invalid_password) {
            if (!connect_process_done) {
                free(status);
                return;
            }

            show_connect_failure(connect_exit_code);
            free(status);
            return;
        }

        free(status);
        return;
    }

    free(status);

    if (get_current_ip()) {
        ui_network_locked = 0;
        connecting_phase = 0;
        connect_grace_ticks = 0;
        connect_settle_ticks = 0;
        return;
    }

    if (!connect_process_done) {
        connecting_phase = 1;
        set_connect_value(lang.muxnetprofile.connect_try);
        return;
    }

    if (connect_exit_code != 0) {
        show_connect_failure(connect_exit_code);
        return;
    }

    if (connect_settle_ticks < CONNECT_SETTLE_TICKS) {
        connect_settle_ticks++;
        connecting_phase = 1;
        set_connect_value(lang.muxnetprofile.connect_try);
        return;
    }

    show_connect_failure(connect_exit_code);
}

static void net_connect_check(const int exit_code) {
    connect_process_done = 1;
    connect_exit_code = exit_code;
    connect_settle_ticks = 0;

    if (exit_code != 0) {
        show_connect_failure(exit_code);
        return;
    }

    ui_network_locked = 1;
    connecting_phase = 1;
    connect_grace_ticks = 0;
    last_status[0] = '\0';

    if (!get_current_ip()) set_connect_value(lang.muxnetprofile.connect_try);
}

static void restore_network_values(void) {
    char *profile_name_raw = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
    viewing_existing_profile = profile_name_raw && *profile_name_raw;

    snprintf(current_profile, sizeof(current_profile), "%s", viewing_existing_profile ? profile_name_raw : "");

    if (viewing_existing_profile) {
        char profile_name_buf[MAX_BUFFER_SIZE];
        snprintf(profile_name_buf, sizeof(profile_name_buf), "%s", profile_name_raw);

        char profile_file[MAX_BUFFER_SIZE];
        const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name_buf);
        if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) return;

        mini_t *net = mini_try_load(profile_file);
        const int is_static = strcasecmp(mini_get_string(net, "network", "type", "dhcp"), "static") == 0;

        const char *ssid = mini_get_string(net, "network", "ssid", "");
        const char *pass = mini_get_string(net, "network", "pass", "");

        int priority = (int) mini_get_int(net, "network", "priority", 5);

        if (priority < 1) priority = 1;
        if (priority > 9) priority = 9;

        char priority_buf[4];
        snprintf(priority_buf, sizeof(priority_buf), "%d", priority);

        lv_label_set_text(ui_val_profile_name_network, profile_name_buf);
        lv_label_set_text(ui_val_type_network, is_static ? lang.muxnetprofile.statc : lang.muxnetprofile.dhcp);
        lv_label_set_text(ui_val_priority_network, priority_buf);
        ui_count_static = is_static ? UI_STATIC : UI_DHCP;
        lv_label_set_text(ui_val_identifier_network, ssid);
        snprintf(pending_password, sizeof(pending_password), "%s", pass);
        lv_label_set_text(ui_val_password_network, *pending_password ? PASS_ENCODE : "");
        lv_label_set_text(ui_val_address_network, mini_get_string(net, "network", "address", ""));
        lv_label_set_text(ui_val_subnet_network, mini_get_string(net, "network", "subnet", ""));
        lv_label_set_text(ui_val_gateway_network, mini_get_string(net, "network", "gateway", ""));
        lv_label_set_text(ui_val_dns_network, mini_get_string(net, "network", "dns", ""));
        mini_free(net);
    } else {
        char ssid_buf[MAX_BUFFER_SIZE];
        char *pending_ssid = read_line_char_from(CONF_CONFIG_PATH "network/ssid", 1);
        snprintf(ssid_buf, sizeof(ssid_buf), "%s", pending_ssid && *pending_ssid ? pending_ssid : "");
        free(pending_ssid);

        lv_label_set_text(ui_val_profile_name_network, ssid_buf);
        lv_label_set_text(ui_val_type_network, lang.muxnetprofile.dhcp);
        lv_label_set_text(ui_val_priority_network, "5");
        ui_count_static = UI_DHCP;
        lv_label_set_text(ui_val_identifier_network, ssid_buf);
        pending_password[0] = '\0';
        lv_label_set_text(ui_val_password_network, "");
        lv_label_set_text(ui_val_address_network, "");
        lv_label_set_text(ui_val_subnet_network, "");
        lv_label_set_text(ui_val_gateway_network, "");
        lv_label_set_text(ui_val_dns_network, "");
    }

    viewing_active_profile = profile_is_active();
    free(profile_name_raw);
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

    if (strcasecmp(lv_label_get_text(ui_val_type_network), lang.muxnetprofile.statc) == 0) idx_type = 1;

    const char *ssid = lv_label_get_text(ui_val_identifier_network);

    char esc_ssid[MAX_BUFFER_SIZE];
    char pass_buf[MAX_BUFFER_SIZE];

    escape_wpa_string(ssid ? ssid : "", esc_ssid);
    snprintf(pass_buf, sizeof(pass_buf), "%s", pending_password);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/type", INT, idx_type);
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid", CHAR, ssid ? ssid : "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid_wpa", CHAR, esc_ssid);
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/pass", CHAR, pass_buf);

    if (idx_type) {
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/address", CHAR, lv_label_get_text(ui_val_address_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/subnet", CHAR, lv_label_get_text(ui_val_subnet_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/gateway", CHAR, lv_label_get_text(ui_val_gateway_network));
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/dns", CHAR, lv_label_get_text(ui_val_dns_network));
    } else {
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/address", CHAR, "");
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/subnet", CHAR, "");
        write_text_to_file_atomic(CONF_CONFIG_PATH "network/gateway", CHAR, "");
    }

    refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, network, profile_name, lang.muxnetprofile.profile_name, "profile_name", "");
    INIT_VALUE_ITEM(-1, network, identifier, lang.muxnetprofile.identifier, "identifier", "");
    INIT_VALUE_ITEM(-1, network, password, lang.muxnetprofile.password, "password", "");
    INIT_VALUE_ITEM(-1, network, type, lang.muxnetprofile.type, "type", "");
    INIT_VALUE_ITEM(-1, network, priority, lang.muxnetprofile.priority, "priority", "");
    INIT_VALUE_ITEM(-1, network, address, lang.muxnetprofile.address, "address", "");
    INIT_VALUE_ITEM(-1, network, subnet, lang.muxnetprofile.subnet, "subnet", "");
    INIT_VALUE_ITEM(-1, network, gateway, lang.muxnetprofile.gateway, "gateway", "");
    INIT_VALUE_ITEM(-1, network, dns, lang.muxnetprofile.dns, "dns", "");
    INIT_VALUE_ITEM(-1, network, connect, lang.muxnetprofile.connect, "connect", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(0, +1, 1, 0, 1);
    nav_moved = 1;

    check_focus();
}

static void check_focus() {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    const int show_lr =
        !viewing_active_profile && (e_focused == ui_lbl_priority_network || e_focused == ui_lbl_type_network);

    if (show_lr) {
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
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

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_profile_name_network) {
        const char *new_profile = lv_textarea_get_text(ui_txt_entry_network);
        if (!new_profile || !*new_profile) new_profile = lv_label_get_text(ui_val_identifier_network);
        lv_label_set_text(ui_val_profile_name_network, new_profile);
    } else if (e_focused == ui_lbl_identifier_network) {
        const char *old_ssid = lv_label_get_text(ui_val_identifier_network);
        const char *cur_profile = lv_label_get_text(ui_val_profile_name_network);
        const char *new_ssid = lv_textarea_get_text(ui_txt_entry_network);

        if (!cur_profile || !*cur_profile || strcmp(cur_profile, old_ssid) == 0)
            lv_label_set_text(ui_val_profile_name_network, new_ssid);
        lv_label_set_text(ui_val_identifier_network, new_ssid);
    } else if (e_focused == ui_lbl_password_network) {
        const char *typed = lv_textarea_get_text(ui_txt_entry_network);
        snprintf(pending_password, sizeof(pending_password), "%s", typed ? typed : "");
        lv_label_set_text(ui_val_password_network, *pending_password ? PASS_ENCODE : "");
    } else if (e_focused == ui_lbl_address_network) {
        lv_label_set_text(ui_val_address_network, lv_textarea_get_text(ui_txt_entry_network));
    } else if (e_focused == ui_lbl_subnet_network) {
        lv_label_set_text(ui_val_subnet_network, lv_textarea_get_text(ui_txt_entry_network));
    } else if (e_focused == ui_lbl_gateway_network) {
        lv_label_set_text(ui_val_gateway_network, lv_textarea_get_text(ui_txt_entry_network));
    } else if (e_focused == ui_lbl_dns_network) {
        lv_label_set_text(ui_val_dns_network, lv_textarea_get_text(ui_txt_entry_network));
    }

    if (lv_obj_has_state(key_entry, LV_STATE_DISABLED)) {
        reset_osk(num_entry);
    } else {
        reset_osk(key_entry);
    }

    lv_textarea_set_text(ui_txt_entry_network, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnl_entry_network);
    fields_modified = 1;
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    lv_obj_t *active = lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN) ? num_entry : key_entry;
    const char *is_key = lv_btnmatrix_get_btn_text(active, key_curr);

    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(active, LV_EVENT_CLICKED, &key_curr);
    }
}

static void toggle_static_panels(const int hide) {
    const struct {
        lv_obj_t *panel;
        lv_obj_t *label;
    } items[] = {
        {ui_pnl_address_network, ui_lbl_address_network},
        {ui_pnl_subnet_network, ui_lbl_subnet_network},
        {ui_pnl_gateway_network, ui_lbl_gateway_network},
        {ui_pnl_dns_network, ui_lbl_dns_network},
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

static void handle_priority_change(const int delta) {
    if (ui_network_locked) return;

    if (viewing_active_profile) {
        play_sound(snd_error);
        toast_message(lang.muxnetprofile.deny_modify, tst_wait_s);
        return;
    }

    int next = safe_atoi(lv_label_get_text(ui_val_priority_network), 5) + delta;

    if (next < 1) next = 9;
    if (next > 9) next = 1;

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", next);
    lv_label_set_text(ui_val_priority_network, buf);

    play_sound(snd_option);
    fields_modified = 1;
}

int handle_navigate(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_priority_network) return 1;

    if (e_focused == ui_lbl_type_network) {
        if (lv_obj_has_flag(ui_lbl_nav_x, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(snd_error);
            toast_message(lang.muxnetprofile.deny_modify, tst_wait_s);

            return 1;
        }

        play_sound(snd_option);

        const int is_static = strcasecmp(lv_label_get_text(ui_val_type_network), lang.muxnetprofile.statc) == 0;
        lv_label_set_text(ui_val_type_network, is_static ? lang.muxnetprofile.dhcp : lang.muxnetprofile.statc);
        ui_count_static = is_static ? UI_DHCP : UI_STATIC;
        toggle_static_panels(is_static);

        return 1;
    }

    return 0;
}

static void handle_confirm(void) {
    if (handle_navigate()) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_connect_network) {
        if (lv_obj_has_flag(ui_lbl_nav_x, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(snd_confirm);
            write_text_to_file_atomic(address_file, CHAR, "");
            clear_current_active_profile();
            run_exec(net_d_args, A_SIZE(net_d_args), 0, 0, NULL, NULL);
            viewing_active_profile = 0;
            can_scan_check(1);
            check_focus();
        } else {
            if (active_profile_blocks_connect()) {
                play_sound(snd_error);
                toast_message(lang.muxnetprofile.connect_deny, tst_wait_s);
                return;
            }

            int valid_info = 0;
            const char *cv_ssid = lv_label_get_text(ui_val_identifier_network);

            char password_buf[MAX_BUFFER_SIZE];
            snprintf(password_buf, sizeof(password_buf), "%s", pending_password);
            const size_t cv_pass_len = strlen(password_buf);

            // wpa2 pass phrases are 8 to 63 bytes long, or 0 bytes for no password; 64 bytes is an
            // already-derived PSK hash carried over unchanged from a previously saved profile
            const int cv_pass_ok = cv_pass_len == 0 || cv_pass_len == 64 || (cv_pass_len >= 8 && cv_pass_len <= 63);

            if (strcasecmp(lv_label_get_text(ui_val_type_network), lang.muxnetprofile.statc) == 0) {
                const char *cv_address = lv_label_get_text(ui_val_address_network);
                const char *cv_subnet = lv_label_get_text(ui_val_subnet_network);
                const char *cv_gateway = lv_label_get_text(ui_val_gateway_network);
                const char *cv_dns = lv_label_get_text(ui_val_dns_network);

                if (strlen(cv_ssid) > 0 && cv_pass_ok && strlen(cv_address) > 0 && strlen(cv_subnet) > 0
                    && strlen(cv_gateway) > 0 && strlen(cv_dns) > 0) {
                    valid_info = 1;
                }
            } else {
                if (strlen(cv_ssid) > 0 && cv_pass_ok) valid_info = 1;
            }

            if (valid_info) {
                play_sound(snd_confirm);
                save_network_config();
                save_profile_ini();
                network_saved = 1;

                if (cv_pass_len > 0) {
                    lv_label_set_text(ui_val_connect_network, lang.muxnetprofile.encrypt_password);
                } else {
                    lv_label_set_text(ui_val_connect_network, lang.muxnetprofile.no_password);
                }

                lv_label_set_text(ui_val_password_network, PASS_ENCODE);
                set_connect_value(lang.muxnetprofile.connect_try);
                lv_task_handler();

                connecting_phase = 1;
                connect_grace_ticks = 0;
                viewing_active_profile = 0;
                write_text_to_file_atomic(CONF_CONFIG_PATH "network/active", CHAR, "");

                ui_network_locked = 1;

                last_status[0] = '\0';
                connect_process_done = 0;
                connect_exit_code = 0;
                connect_grace_ticks = 0;
                connect_settle_ticks = 0;

                clear_profile_status_file();

                run_exec(pass_args, A_SIZE(pass_args), 0, 0, NULL, NULL);

                memset(password_buf, 0, sizeof(password_buf));
                lv_textarea_set_text(ui_txt_entry_network, "");

                lv_task_handler();

                // Connect the specific profile so the script tracks its active status independently!
                const char *net_c_prof_args[] = {OPT_PATH NET_SCRIPT, "connect", current_profile, NULL};
                run_exec(net_c_prof_args, A_SIZE(net_c_prof_args), 1, 0, NULL, net_connect_check);
                lv_task_handler();
            } else {
                play_sound(snd_error);
                toast_message(lang.muxnetprofile.check, tst_wait_s);
            }
        }
    } else {
        if (!lv_obj_has_flag(ui_lbl_nav_x, LV_OBJ_FLAG_HIDDEN)) {
            play_sound(snd_confirm);
            key_curr = 0;
            if (e_focused == ui_lbl_profile_name_network || e_focused == ui_lbl_identifier_network
                || e_focused == ui_lbl_password_network) {
                lv_textarea_set_password_mode(ui_txt_entry_network, e_focused == ui_lbl_password_network);

                lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

                lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_state(num_entry, LV_STATE_DISABLED);

                key_show = 1;
            } else {
                lv_textarea_set_password_mode(ui_txt_entry_network, 0);

                lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_state(num_entry, LV_STATE_DISABLED);

                lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_state(key_entry, LV_STATE_DISABLED);

                key_show = 2;
            }

            osk_show(ui_pnl_entry_network);
            osk_refresh_labels();

            lv_textarea_set_text(
                ui_txt_entry_network,
                e_focused == ui_lbl_password_network ? "" : lv_label_get_text(lv_group_get_focused(ui_group_value))
            );
        } else {
            play_sound(snd_error);
            toast_message(lang.muxnetprofile.deny_modify, tst_wait_s);
        }
    }
}

static void handle_back(void) {
    if (fields_modified && !network_saved) {
        play_sound(snd_confirm);
        dialogue_open(&save_dlg_active, &save_dlg, &theme);
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_profile");

    mux_input_stop();
}

static void handle_scan(void) {
    if (ui_network_locked) return;

    if (!lv_obj_has_flag(ui_lbl_nav_x, LV_OBJ_FLAG_HIDDEN)) {
        play_sound(snd_confirm);

        save_network_config();
        load_mux("net_scan");

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_obj_get_user_data(lv_group_get_focused(ui_group)));

        mux_input_stop();
    }
}

static void do_forget_profile(void) {
    char *profile_name_raw = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
    if (!profile_name_raw || !*profile_name_raw) {
        free(profile_name_raw);
        return;
    }

    char profile_file[MAX_BUFFER_SIZE];
    const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name_raw);
    if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) {
        free(profile_name_raw);
        return;
    }

    remove(profile_file);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, "");
    viewing_existing_profile = 0;
    free(profile_name_raw);
}

static int derive_psk(const char *ssid, const char *pass, char *out) {
    if (!ssid || !*pass || strlen(pass) == 64) return 0;

    int pfd[2];
    if (pipe(pfd) != 0) return 0;

    const pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return 0;
    }

    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);

        const int dn = open("/dev/null", O_WRONLY);
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

    const char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, "psk=", 4) == 0) {
            const char *psk = p + 4;
            const char *eol = strchr(psk, '\n');
            const size_t psk_len = eol ? (size_t) (eol - psk) : strlen(psk);
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
    const char *ssid = lv_label_get_text(ui_val_identifier_network);
    if (!ssid || !*ssid) return;

    const char *profile_name_raw = lv_label_get_text(ui_val_profile_name_network);
    char profile_name[MAX_BUFFER_SIZE];

    int name_ok;
    if (profile_name_raw && *profile_name_raw) {
        name_ok = sanitise_ssid_name(profile_name, profile_name_raw);
    } else {
        name_ok = sanitise_ssid_name(profile_name, ssid);
    }

    if (!name_ok) return;

    mkdir(STORAGE_NETWORK, 0755);

    if (viewing_existing_profile) {
        char *old_name = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
        if (old_name && *old_name && strcmp(old_name, profile_name) != 0) {
            char old_file[MAX_BUFFER_SIZE];
            const int of_len = snprintf(old_file, sizeof(old_file), STORAGE_NETWORK "/%s.ini", old_name);
            if (of_len >= 0 && (size_t) of_len < sizeof(old_file)) remove(old_file);
        }
        free(old_name);
    }

    char profile_file[MAX_BUFFER_SIZE];
    const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);
    if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) return;

    const int is_static = strcasecmp(lv_label_get_text(ui_val_type_network), lang.muxnetprofile.statc) == 0;

    mini_t *net = mini_try_load(profile_file);

    char pass_buf[MAX_BUFFER_SIZE];
    snprintf(pass_buf, sizeof(pass_buf), "%s", pending_password);

    if (*pass_buf && strlen(pass_buf) != 64) {
        char psk[65] = {0};
        if (derive_psk(ssid, pass_buf, psk)) snprintf(pass_buf, sizeof(pass_buf), "%s", psk);
    }

    mini_set_string(net, "network", "ssid", ssid);
    mini_set_string(net, "network", "pass", pass_buf);
    mini_set_string(net, "network", "type", is_static ? "static" : "dhcp");

    if (is_static) {
        mini_set_string(net, "network", "address", lv_label_get_text(ui_val_address_network));
        mini_set_string(net, "network", "subnet", lv_label_get_text(ui_val_subnet_network));
        mini_set_string(net, "network", "gateway", lv_label_get_text(ui_val_gateway_network));
        mini_set_string(net, "network", "dns", lv_label_get_text(ui_val_dns_network));
    }

    mini_set_int(net, "network", "autoconnect", (int) mini_get_int(net, "network", "autoconnect", 1));

    int pri = safe_atoi(lv_label_get_text(ui_val_priority_network), 0);
    if (pri < 0) pri = 0;
    if (pri > 9) pri = 9;

    mini_set_int(net, "network", "priority", pri);
    mini_save(net, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, profile_name);
    snprintf(current_profile, sizeof(current_profile), "%s", profile_name);
    viewing_existing_profile = 1;

    lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void handle_a(void) {
    if (forget_dlg_active) {
        const mux_confirm_opt opt = (mux_confirm_opt) forget_dlg.selected;
        dialogue_dismiss(&forget_dlg_active, &forget_dlg);

        if (opt == mux_confirm_yep) {
            do_forget_profile();

            play_sound(snd_confirm);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_profile");

            mux_input_stop();
        }

        return;
    }

    if (save_dlg_active) {
        const mux_confirm_opt opt = (mux_confirm_opt) save_dlg.selected;
        dialogue_dismiss(&save_dlg_active, &save_dlg);

        if (opt == mux_confirm_yep) {
            if (viewing_active_profile) save_network_config();
            save_profile_ini();
        }

        play_sound(snd_back);
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
        dialogue_dismiss(&forget_dlg_active, &forget_dlg);

        play_sound(snd_back);
        return;
    }

    if (save_dlg_active) {
        dialogue_dismiss(&save_dlg_active, &save_dlg);

        play_sound(snd_back);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_network);
        return;
    }

    handle_back();
}

static void handle_b_hold(void) {
    if (forget_dlg_active || save_dlg_active) return;

    if (key_show) key_backspace(ui_txt_entry_network);
}

static void handle_x(void) {
    if (forget_dlg_active || msgbox_active || hold_call || ui_network_locked) return;

    if (key_show) {
        close_osk(
            lv_obj_has_state(key_entry, LV_STATE_DISABLED) ? num_entry : key_entry, ui_group, ui_txt_entry_network,
            ui_pnl_entry_network
        );
        return;
    }

    handle_scan();
}

static void handle_y(void) {
    if (msgbox_active || hold_call || ui_network_locked) return;

    if (key_show == 1) {
        key_space(ui_txt_entry_network);
        return;
    }

    if (key_show == 2 || !viewing_existing_profile) return;

    if (viewing_active_profile) {
        play_sound(snd_error);
        toast_message(lang.muxnetprofile.deny_forget, tst_wait_s);
        return;
    }

    play_sound(snd_info_open);
    dialogue_open(&forget_dlg_active, &forget_dlg, &theme);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || key_show || hold_call || ui_network_locked)
        return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    if (forget_dlg_active) {
        dialogue_handle_dpad(&forget_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (save_dlg_active) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
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
        dialogue_handle_dpad(&forget_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (save_dlg_active) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
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
        dialogue_handle_dpad(&forget_dlg, &theme, -1, swap_axis);
        return;
    }

    if (save_dlg_active) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    if (ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lbl_priority_network) {
        handle_priority_change(-1);
        return;
    }

    key_show ? key_left() : handle_navigate();
}

static void handle_right(void) {
    if (forget_dlg_active) {
        dialogue_handle_dpad(&forget_dlg, &theme, +1, swap_axis);
        return;
    }

    if (save_dlg_active) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    if (ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lbl_priority_network) {
        handle_priority_change(+1);
        return;
    }

    key_show ? key_right() : handle_navigate();
}

static void handle_left_hold(void) {
    if (forget_dlg_active || save_dlg_active || ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lbl_priority_network) {
        handle_priority_change(-1);
        return;
    }

    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (forget_dlg_active || save_dlg_active || ui_network_locked) return;

    if (!key_show && lv_group_get_focused(ui_group) == ui_lbl_priority_network) {
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
    char status_path[MAX_BUFFER_SIZE];
    profile_status_path(status_path);
    FILE *f = fopen(status_path, "r");
    if (!f) return;

    char status[64] = {0};
    const int ok = fgets(status, sizeof(status), f) != NULL;
    fclose(f);

    if (!ok) return;

    char *p = status;
    while (*p && *p != '\n' && *p != '\r')
        p++;
    *p = '\0';

    const int id = resolve_status_id(status);
    if (id == status_associating || id == status_authenticating || id == status_waiting_ip || id == status_validating) {
        ui_network_locked = 1;
        connecting_phase = 1;
    }
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_entry_network,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, ui_pnl_message, NULL});
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.scan, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.muxnetprofile.forget, 0},
      {NULL, NULL, 0}});

    lv_obj_t *connect_items[] = {ui_pnl_profile_name_network, ui_pnl_identifier_network, ui_pnl_password_network,
                                 ui_pnl_type_network,         ui_pnl_connect_network,    NULL};
    for (int i = 0; connect_items[i]; i++)
        lv_obj_clear_flag(connect_items[i], LV_OBJ_FLAG_HIDDEN);

    toggle_static_panels(!config.network.type);

#define NETWORK(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_network, UDATA);
    NETWORK_ELEMENTS
#undef NETWORK

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (ui_network_locked) {
        exec_watch_task();
        update_network_label();
    }

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_panels();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_ok_press();
    ev.code == KEY_ESC &&ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txt_entry_network);
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

    init_ui_common_screen(&theme, &device, &lang, lang.muxnetprofile.title);
    init_muxnetwork(ui_screen, ui_pnl_content, &theme);
    init_elements();

    snprintf(address_file, sizeof(address_file), CONF_CONFIG_PATH "/network/address");

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_network_values();
    check_connecting_state();

    if (!viewing_existing_profile) {
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    init_osk(ui_pnl_entry_network, ui_txt_entry_network, 1, 1, OSK_MAX);
    can_scan_check(viewing_active_profile ? 0 : 1);

    if (viewing_active_profile) {
        char ip[IP_OCTET] = {0};
        if (read_ip(ip) && *ip && strcasecmp(ip, "0.0.0.0") != 0) {
            set_connect_value(ip);
        } else {
            set_connect_value(lang.muxnetprofile.connected);
        }
    }

    dialogue_init_confirm(
        &save_dlg, &theme, ui_screen, lang.generic.confirm, NULL, lang.generic.save, lang.generic.cancel,
        lang.generic.select, lang.generic.back
    );

    dialogue_init_confirm(
        &forget_dlg, &theme, ui_screen, lang.generic.confirm, lang.muxnetprofile.forget_confirm,
        lang.muxnetprofile.forget, lang.generic.cancel, lang.generic.select, lang.generic.back
    );

    init_timer(ui_refresh_task, NULL);

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
         }};

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);

    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
