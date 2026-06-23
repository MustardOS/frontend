#include "muxshare.h"

static void sanitise_ssid_name(char *dest, const char *src) {
    size_t j = 0;
    while (*src && j < MAX_BUFFER_SIZE - 1) {
        dest[j++] = (*src == '/' || *src == '\\') ? '_' : *src;
        src++;
    }
    dest[j] = '\0';
}

static void show_help(void) {
    show_info_box(lang.MUXNETWORK.TITLE, lang.MUXNETWORK.HELP, 0);
}

static const char *get_profile_status(const char *profile_name) {
    static char profile_file[MAX_BUFFER_SIZE];
    static char status_buf[MAX_BUFFER_SIZE];
    char profile_ssid[MAX_BUFFER_SIZE];

    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);

    mini_t *net = mini_try_load(profile_file);
    snprintf(profile_ssid, sizeof(profile_ssid), "%s", mini_get_string(net, "network", "ssid", ""));

    int autoconnect = (int) mini_get_int(net, "network", "autoconnect", 1);
    int priority = (int) mini_get_int(net, "network", "priority", 5);

    mini_free(net);

    if (priority < 0) priority = 0;
    if (priority > 9) priority = 9;

    const char *active = config.NETWORK.SSID;
    if (active && *active && strcmp(profile_ssid, active) == 0 && is_network_connected()) {
        snprintf(status_buf, sizeof(status_buf), "%s (%s)", lang.MUXNETWORK.CONNECTED, autoconnect ? lang.MUXNETWORK.AUTO : lang.MUXNETWORK.MANUAL);
        return status_buf;
    }

    snprintf(status_buf, sizeof(status_buf), "%s (%d)", autoconnect ? lang.MUXNETWORK.AUTO : lang.MUXNETWORK.MANUAL, priority);
    return status_buf;
}

struct profile_entry {
    char name[MAX_BUFFER_SIZE];
    int priority;
};

static int profile_entry_compare(const void *a, const void *b) {
    const struct profile_entry *pa = (const struct profile_entry *) a;
    const struct profile_entry *pb = (const struct profile_entry *) b;

    if (pa->priority != pb->priority) return pa->priority - pb->priority;

    return strcasecmp(pa->name, pb->name);
}

static void populate_profile_list(void) {
    const char *dirs[] = {STORAGE_NETWORK};
    const char *exts[] = {".ini"};

    char **files = NULL;
    size_t file_count = 0;

    if (scan_directory_list(dirs, exts, &files, A_SIZE(dirs), A_SIZE(exts), &file_count) < 0) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
        return;
    }

    if (file_count == 0) {
        free_array(files, file_count);
        return;
    }

    struct profile_entry *entries = malloc(file_count * sizeof(struct profile_entry));
    if (!entries) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
        free_array(files, file_count);
        return;
    }

    for (size_t i = 0; i < file_count; ++i) {
        char *base_filename = files[i];

        char profile_name[MAX_BUFFER_SIZE];
        snprintf(profile_name, sizeof(profile_name), "%s", str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        snprintf(entries[i].name, sizeof(entries[i].name), "%s", strip_ext(profile_name));

        char profile_file[MAX_BUFFER_SIZE];
        snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", entries[i].name);

        mini_t *net = mini_try_load(profile_file);
        int pri = (int) mini_get_int(net, "network", "priority", 5);
        mini_free(net);

        if (pri < 0) pri = 0;
        if (pri > 9) pri = 9;
        entries[i].priority = pri;
    }

    free_array(files, file_count);

    qsort(entries, file_count, sizeof(struct profile_entry), profile_entry_compare);

    for (size_t i = 0; i < file_count; ++i) {
        const char *profile_store = entries[i].name;
        const char *status = get_profile_status(profile_store);

        ui_count++;

        lv_obj_t *ui_pnlProfile = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlProfile);
        lv_obj_set_user_data(ui_pnlProfile, strdup(profile_store));

        lv_obj_t *ui_lblProfile = lv_label_create(ui_pnlProfile);
        apply_theme_list_item(&theme, ui_lblProfile, profile_store);

        lv_obj_t *ui_lblProfileStatus = lv_label_create(ui_pnlProfile);
        apply_theme_list_value(&theme, ui_lblProfileStatus, status);

        lv_obj_t *ui_icoProfile = lv_img_create(ui_pnlProfile);
        apply_theme_list_glyph(&theme, ui_icoProfile, mux_module, "profile");

        lv_group_add_obj(ui_group, ui_lblProfile);
        lv_group_add_obj(ui_group_value, ui_lblProfileStatus);
        lv_group_add_obj(ui_group_glyph, ui_icoProfile);
        lv_group_add_obj(ui_group_panel, ui_pnlProfile);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblProfile, ui_icoProfile, profile_store);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblProfile);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(entries);
}

static void handle_a(void) {
    if (hold_call) return;

    if (msgbox_active || !ui_count) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *profile_name = (const char *) lv_obj_get_user_data(panel);
    if (!profile_name) return;

    play_sound(SND_CONFIRM);
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

    play_sound(SND_BACK);
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);
    load_mux("net_scan");
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    play_sound(SND_CONFIRM);

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
    if (msgbox_active || !ui_count || hold_call) return;

    lv_obj_t *panel = lv_group_get_focused(ui_group_panel);
    if (!panel) return;

    const char *profile_name = (const char *) lv_obj_get_user_data(panel);
    if (!profile_name) return;

    char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);

    mini_t *net = mini_try_load(profile_file);

    int autoconnect = !((int) mini_get_int(net, "network", "autoconnect", 1));
    int priority = (int) mini_get_int(net, "network", "priority", 5);

    char profile_ssid[MAX_BUFFER_SIZE];
    snprintf(profile_ssid, sizeof(profile_ssid), "%s", mini_get_string(net, "network", "ssid", ""));

    mini_set_int(net, "network", "autoconnect", autoconnect);
    mini_save(net, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net);

    play_sound(SND_OPTION);

    lv_obj_t *val = lv_group_get_focused(ui_group_value);
    if (!val) return;

    if (priority < 0) priority = 0;
    if (priority > 9) priority = 9;

    char status[MAX_BUFFER_SIZE];
    const char *active = config.NETWORK.SSID;
    const char *ac_str = autoconnect ? lang.MUXNETWORK.AUTO : lang.MUXNETWORK.MANUAL;

    if (active && *active && strcmp(profile_ssid, active) == 0 && is_network_connected()) {
        snprintf(status, sizeof(status), "%s (%s)", lang.MUXNETWORK.CONNECTED, ac_str);
    } else {
        snprintf(status, sizeof(status), "%s (%d)", ac_str, priority);
    }

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
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_navigation_group(void) {
    reset_ui_groups();
    populate_profile_list();

    if (ui_count > 0) {
        int target = 0;
        char *saved_name = read_line_char_from(CONF_CONFIG_PATH "network/profile_name", 1);
        if (saved_name && *saved_name) {
            uint32_t count = lv_obj_get_child_cnt(ui_pnlContent);
            for (uint32_t i = 0; i < count; i++) {
                lv_obj_t *child = lv_obj_get_child(ui_pnlContent, (int32_t) i);
                const char *data = (const char *) lv_obj_get_user_data(child);
                if (data && strcmp(data, saved_name) == 0) {
                    target = (int) i;
                    break;
                }
            }
        }
        gen_step_movement(target, +1, 1, 0, 1);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  1},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 1},
            {ui_lblNavAGlyph,  "",                  1},
            {ui_lblNavA,       lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.SCAN,   0},
            {ui_lblNavYGlyph,  "",                  0},
            {ui_lblNavY,       lang.GENERIC.NEW,    0},
            {NULL, NULL,                            0}
    });

    overlay_display();
}

int muxnetwork_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETWORK.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    if (ui_count == 0) lv_label_set_text(ui_lblScreenMessage, lang.MUXNETWORK.NONE);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, true);

    mux_input_task(&input_opts);

    return 0;
}
