#include "muxshare.h"

static void show_help(void) {
    show_info_box(lang.muxnetwork.title, lang.muxnetwork.help, 0);
}

static void net_trim(char *value) {
    if (!value) return;

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

static int profile_matches_connected_ssid(const char *profile_name, const char *ssid) {
    if (!*profile_name || !*ssid) return 0;

    if (strcmp(profile_name, ssid) == 0) return 1;

    char profile_file[MAX_BUFFER_SIZE];
    const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);
    if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) return 0;

    mini_t *net = mini_try_load(profile_file);
    const char *profile_ssid = mini_get_string(net, "network", "ssid", "");
    const int match = profile_ssid && *profile_ssid && strcmp(profile_ssid, ssid) == 0;
    mini_free(net);

    return match;
}

static int find_connected_profile(char *buf) {
    buf[0] = '\0';

    char ssid[MAX_BUFFER_SIZE];
    if (!read_connected_ssid(ssid)) return 0;

    const char *dirs[] = {STORAGE_NETWORK};
    const char *exts[] = {".ini"};

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, exts, &files, A_SIZE(dirs), A_SIZE(exts), &file_count) < 0) return 0;

    int found = 0;

    for (size_t i = 0; i < file_count; ++i) {
        const char *base_filename = files[i];

        char profile_name[MAX_BUFFER_SIZE];
        snprintf(
            profile_name, sizeof(profile_name), "%s",
            str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/')
        );

        char profile_store[MAX_BUFFER_SIZE];
        snprintf(profile_store, sizeof(profile_store), "%s", strip_ext(profile_name));

        if (profile_matches_connected_ssid(profile_store, ssid)) {
            snprintf(buf, MAX_BUFFER_SIZE, "%s", profile_store);
            found = 1;
            break;
        }
    }

    free_array(files, file_count);
    return found;
}

// Formats the profile status line...
//   Auto, connected     -> "Connected - Auto (3)"
//   Auto, not connected -> "Auto (3)"
//   Manual, connected   -> "Connected - Manual"
//   Manual              -> "Manual"
static void format_profile_status(char *buf, const int autoconnect, const int priority, const int connected) {
    char base[MAX_BUFFER_SIZE];

    if (autoconnect) {
        snprintf(base, sizeof(base), "%s (%d)", lang.muxnetwork.autom, priority);
    } else {
        snprintf(base, sizeof(base), "%s", lang.muxnetwork.manual);
    }

    if (connected) {
        snprintf(buf, MAX_BUFFER_SIZE, "%s - %s", lang.muxnetwork.connected, base);
    } else {
        snprintf(buf, MAX_BUFFER_SIZE, "%s", base);
    }
}

static const char *get_profile_status(const char *profile_name, const char *active_profile) {
    static char profile_file[MAX_BUFFER_SIZE];
    static char status_buf[MAX_BUFFER_SIZE];

    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);

    mini_t *net = mini_try_load(profile_file);

    const int autoconnect = (int) mini_get_int(net, "network", "autoconnect", 1);
    int priority = (int) mini_get_int(net, "network", "priority", 5);

    mini_free(net);

    if (priority < 1) priority = 1;
    if (priority > 9) priority = 9;

    const int connected = *active_profile && strcmp(profile_name, active_profile) == 0;
    format_profile_status(status_buf, autoconnect, priority, connected);

    return status_buf;
}

static void read_active_profile(char *buf) {
    buf[0] = '\0';

    char state[MAX_BUFFER_SIZE];
    if (read_wpa_status_value("wpa_state", state)) {
        if (strcmp(state, "COMPLETED") != 0) {
            write_text_to_file_atomic(CONF_CONFIG_PATH "network/active", CHAR, "");
            return;
        }

        if (find_connected_profile(buf)) {
            write_text_to_file_atomic(CONF_CONFIG_PATH "network/active", CHAR, buf);
            return;
        }

        return;
    }

    char *active = read_line_char_from(CONF_CONFIG_PATH "network/active", 1);
    if (active && *active) {
        snprintf(buf, MAX_BUFFER_SIZE, "%s", active);
        net_trim(buf);
    }
    free(active);
}

typedef struct {
    char name[MAX_BUFFER_SIZE];
    int priority;
    int autoconnect;
} profile_entry;

static int profile_entry_compare(const void *a, const void *b) {
    const profile_entry *pa = a;
    const profile_entry *pb = b;

    // Manual profiles always sort below Auto ones (autoconnect 1 before 0)
    if (pa->autoconnect != pb->autoconnect) return pb->autoconnect - pa->autoconnect;

    // Auto: by priority then name. Manual: priority is meaningless, so by name only.
    if (pa->autoconnect && pa->priority != pb->priority) return pa->priority - pb->priority;

    return strcasecmp(pa->name, pb->name);
}

static void populate_profile_list(void) {
    const char *dirs[] = {STORAGE_NETWORK};
    const char *exts[] = {".ini"};

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, exts, &files, A_SIZE(dirs), A_SIZE(exts), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        return;
    }

    if (file_count == 0) {
        free_array(files, file_count);
        return;
    }

    profile_entry *entries = malloc(file_count * sizeof(profile_entry));
    if (!entries) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_allocate_mem);
        free_array(files, file_count);
        return;
    }

    for (size_t i = 0; i < file_count; ++i) {
        const char *base_filename = files[i];

        char profile_name[MAX_BUFFER_SIZE];
        snprintf(
            profile_name, sizeof(profile_name), "%s",
            str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/')
        );

        snprintf(entries[i].name, sizeof(entries[i].name), "%s", strip_ext(profile_name));

        char profile_file[MAX_BUFFER_SIZE];
        snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", entries[i].name);

        mini_t *net = mini_try_load(profile_file);
        int pri = (int) mini_get_int(net, "network", "priority", 5);
        entries[i].autoconnect = (int) mini_get_int(net, "network", "autoconnect", 1);
        mini_free(net);

        if (pri < 0) pri = 0;
        if (pri > 9) pri = 9;
        entries[i].priority = pri;
    }

    free_array(files, file_count);

    qsort(entries, file_count, sizeof(profile_entry), profile_entry_compare);

    char active_profile[MAX_BUFFER_SIZE];
    read_active_profile(active_profile);

    for (size_t i = 0; i < file_count; ++i) {
        const char *profile_store = entries[i].name;
        const char *status = get_profile_status(profile_store, active_profile);

        ui_count_static++;

        lv_obj_t *ui_pnl_profile = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_profile);
        lv_obj_set_user_data(ui_pnl_profile, strdup(profile_store));

        lv_obj_t *ui_lbl_profile = lv_label_create(ui_pnl_profile);
        apply_theme_list_item(&theme, ui_lbl_profile, profile_store);

        lv_obj_t *ui_lbl_profile_status = lv_label_create(ui_pnl_profile);
        apply_theme_list_value(&theme, ui_lbl_profile_status, status);

        lv_obj_t *ui_ico_profile = lv_img_create(ui_pnl_profile);
        apply_theme_list_glyph(&theme, ui_ico_profile, mux_module, "profile");

        lv_group_add_obj(ui_group, ui_lbl_profile);
        lv_group_add_obj(ui_group_value, ui_lbl_profile_status);
        lv_group_add_obj(ui_group_glyph, ui_ico_profile);
        lv_group_add_obj(ui_group_panel, ui_pnl_profile);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_profile, ui_ico_profile, profile_store);
        apply_text_long_dot(&theme, ui_lbl_profile);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
    free(entries);
}

static void handle_a(void) {
    if (hold_call) return;

    if (msgbox_active || !ui_count_static) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *profile_name = lv_obj_get_user_data(panel);
    if (!profile_name) return;

    play_sound(snd_confirm);
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, profile_name);

    load_mux("network");
    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);
    load_mux("net_scan");
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    play_sound(snd_confirm);

    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/ssid_wpa", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/pass", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/type", INT, 0);
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/address", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/subnet", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/gateway", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/dns", CHAR, "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "network/profile_name", CHAR, "");

    load_mux("network");
    mux_input_stop();
}

static void toggle_focused_autoconnect(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *profile_name = lv_obj_get_user_data(panel);
    if (!profile_name) return;

    char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);

    mini_t *net = mini_try_load(profile_file);

    const int autoconnect = !(int) mini_get_int(net, "network", "autoconnect", 1);
    int priority = (int) mini_get_int(net, "network", "priority", 5);

    mini_set_int(net, "network", "autoconnect", autoconnect);
    mini_save(net, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net);

    play_sound(snd_option);

    lv_obj_t *val = lv_group_get_focused(ui_group_value);
    if (!val) return;

    if (priority < 1) priority = 1;
    if (priority > 9) priority = 9;

    char active_profile[MAX_BUFFER_SIZE];
    read_active_profile(active_profile);

    char status[MAX_BUFFER_SIZE];
    const int connected = *active_profile && strcmp(profile_name, active_profile) == 0;
    format_profile_status(status, autoconnect, priority, connected);

    lv_label_set_text(val, status);
}

static void handle_option_prev(void) {
    toggle_focused_autoconnect();
}

static void handle_option_next(void) {
    toggle_focused_autoconnect();
}

static void handle_dpad_up_hold(void) {
    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_navigation_group(void) {
    reset_ui_groups();
    populate_profile_list();

    if (ui_count_static > 0) {
        int target = 0;
        char *saved_name = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
        if (saved_name && *saved_name) {
            net_trim(saved_name);

            const uint32_t count = lv_obj_get_child_cnt(ui_pnl_content);
            for (uint32_t i = 0; i < count; i++) {
                lv_obj_t *child = lv_obj_get_child(ui_pnl_content, (int32_t) i);
                const char *data = lv_obj_get_user_data(child);
                if (data && strcmp(data, saved_name) == 0) {
                    target = (int) i;
                    break;
                }
            }
        }
        free(saved_name);

        gen_step_movement(target, +1, 1, 0, 1);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 1},
                                  {ui_lbl_nav_lr, lang.generic.change, 1},
                                  {ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.select, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.scan, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.new, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxnetwork_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxnetwork.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    if (ui_count_static == 0) lv_label_set_text(ui_lbl_screen_message, lang.muxnetwork.none);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        },
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);

    mux_input_task(&input_opts);

    return 0;
}
