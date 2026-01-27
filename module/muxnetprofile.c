#include "muxshare.h"

static void show_help(void) {
    show_info_box(lang.MUXNETPROFILE.TITLE, lang.MUXNETPROFILE.HELP, 0);
}

static void sanitise_ssid_name(char *dest, const char *src) {
    size_t j = 0;
    while (*src && j < MAX_BUFFER_SIZE - 1) {
        dest[j++] = (*src == '/' || *src == '\\') ? '_' : *src;
        src++;
    }
    dest[j] = '\0';
}

static int remove_profile(char *name) {
    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), RUN_STORAGE_PATH "network/%s.ini",
             name);

    if (file_exist(profile_file)) {
        remove(profile_file);
        return 1;
    }

    return 0;
}

static void load_profile(char *name) {
    toast_message(lang.GENERIC.LOADING, FOREVER);

    static char profile_file[MAX_BUFFER_SIZE];
    snprintf(profile_file, sizeof(profile_file), RUN_STORAGE_PATH "network/%s.ini",
             name);

    mini_t *net_profile = mini_try_load(profile_file);

    write_text_to_file(CONF_CONFIG_PATH "network/type", "w", INT,
                       (strcasecmp(mini_get_string(net_profile, "network", "type", "dhcp"), "static") == 0) ? 1 : 0);

    write_text_to_file(CONF_CONFIG_PATH "network/ssid", "w", CHAR,
                       mini_get_string(net_profile, "network", "ssid", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/hidden", "w", INT,
                       mini_get_int(net_profile, "network", "hidden", 0));

    write_text_to_file(CONF_CONFIG_PATH "network/pass", "w", CHAR,
                       mini_get_string(net_profile, "network", "pass", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/address", "w", CHAR,
                       mini_get_string(net_profile, "network", "address", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/subnet", "w", CHAR,
                       mini_get_string(net_profile, "network", "subnet", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/gateway", "w", CHAR,
                       mini_get_string(net_profile, "network", "gateway", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/dns", "w", CHAR,
                       mini_get_string(net_profile, "network", "dns", ""));

    write_text_to_file(CONF_CONFIG_PATH "network/hostname", "w", CHAR,
                       mini_get_string(net_profile, "network", "hostname", read_line_char_from("/etc/hostname", 1)));

    mini_free(net_profile);
}

static int save_profile(void) {
    toast_message(lang.GENERIC.SAVING, FOREVER);

    const char *p_type = read_all_char_from((CONF_CONFIG_PATH "network/type"));
    const char *p_ssid = read_all_char_from((CONF_CONFIG_PATH "network/ssid"));
    const char *p_pass = read_all_char_from((CONF_CONFIG_PATH "network/pass"));
    const char *p_hidden = read_all_char_from((CONF_CONFIG_PATH "network/hidden"));
    const char *p_address = read_all_char_from((CONF_CONFIG_PATH "network/address"));
    const char *p_subnet = read_all_char_from((CONF_CONFIG_PATH "network/subnet"));
    const char *p_gateway = read_all_char_from((CONF_CONFIG_PATH "network/gateway"));
    const char *p_dns = read_all_char_from((CONF_CONFIG_PATH "network/dns"));
    const char *p_hostname = read_line_char_from("/etc/hostname", 1);

    if (!p_ssid || strlen(p_ssid) == 0) {
        toast_message(lang.MUXNETPROFILE.INVALID_SSID, SHORT);
        return 0;
    }

    int type = safe_atoi(p_type);
    if (type) {
        if (!p_address || strlen(p_address) == 0 ||
            !p_subnet || strlen(p_subnet) == 0 ||
            !p_gateway || strlen(p_gateway) == 0 ||
            !p_dns || strlen(p_dns) == 0) {
            toast_message(lang.MUXNETPROFILE.INVALID_NETWORK, SHORT);
            return 0;
        }
    } else {
        p_address = "";
        p_subnet = "";
        p_gateway = "";
        p_dns = "";
    }

    static char profile_file[MAX_BUFFER_SIZE];
    int counter = 1;

    char sanitised_ssid[MAX_BUFFER_SIZE];
    sanitise_ssid_name(sanitised_ssid, p_ssid);

    snprintf(profile_file, sizeof(profile_file), RUN_STORAGE_PATH "network/%s.ini",
             sanitised_ssid);

    while (file_exist(profile_file)) {
        snprintf(profile_file, sizeof(profile_file), RUN_STORAGE_PATH "network/%s - %d.ini",
                 sanitised_ssid, ++counter);
    }

    mini_t *net_profile = mini_try_load(profile_file);

    mini_set_string(net_profile, "network", "ssid", p_ssid);
    mini_set_string(net_profile, "network", "pass", p_pass);
    mini_set_string(net_profile, "network", "scan", p_hidden);
    mini_set_string(net_profile, "network", "type", (!type) ? "dhcp" : "static");
    mini_set_string(net_profile, "network", "address", (!type) ? "" : p_address);
    mini_set_string(net_profile, "network", "subnet", (!type) ? "" : p_subnet);
    mini_set_string(net_profile, "network", "gateway", (!type) ? "" : p_gateway);
    mini_set_string(net_profile, "network", "dns", (!type) ? "" : p_dns);
    mini_set_string(net_profile, "network", "hostname", p_hostname);

    mini_save(net_profile, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(net_profile);

    return 1;
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void create_profile_items(void) {
    const char *dirs[] = {RUN_STORAGE_PATH "network"};
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

    qsort(files, file_count, sizeof(char *), str_compare);
    reset_ui_groups();

    for (size_t i = 0; i < file_count; ++i) {
        assert(files[i] != NULL);
        char *base_filename = files[i];

        char profile_name[MAX_BUFFER_SIZE];
        snprintf(profile_name, sizeof(profile_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char profile_store[MAX_BUFFER_SIZE];
        snprintf(profile_store, sizeof(profile_store), "%s",
                 strip_ext(profile_name));

        ui_count++;
        add_item(&items, &item_count, profile_store, profile_store, "", ITEM);

        lv_obj_t *ui_pnlProfile = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlProfile);
        lv_obj_set_user_data(ui_pnlProfile, strdup(profile_store));

        lv_obj_t *ui_lblProfileItem = lv_label_create(ui_pnlProfile);
        apply_theme_list_item(&theme, ui_lblProfileItem, profile_store);

        lv_obj_t *ui_lblProfileItemGlyph = lv_img_create(ui_pnlProfile);
        apply_theme_list_glyph(&theme, ui_lblProfileItemGlyph, mux_module, "profile");

        lv_group_add_obj(ui_group, ui_lblProfileItem);
        lv_group_add_obj(ui_group_glyph, ui_lblProfileItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlProfile);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblProfileItem, ui_lblProfileItemGlyph, profile_store);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblProfileItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free_array(files, file_count);
}

static void handle_a(void) {
    if (msgbox_active || is_network_connected() || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);
    load_profile(lv_label_get_text(lv_group_get_focused(ui_group)));

    refresh_config = 1;

    close_input();
    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_save(void) {
    if (msgbox_active || hold_call) return;

    if (save_profile()) {
        play_sound(SND_CONFIRM);
        load_mux("net_profile");

        close_input();
        mux_input_stop();
    }
}

static void handle_remove(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    if (remove_profile(lv_label_get_text(lv_group_get_focused(ui_group)))) {
        play_sound(SND_CONFIRM);
        load_mux("net_profile");

        close_input();
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.LOAD,   1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  0},
            {ui_lblNavX,      lang.GENERIC.SAVE,   0},
            {ui_lblNavYGlyph, "",                  1},
            {ui_lblNavY,      lang.GENERIC.REMOVE, 1},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

int muxnetprofile_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETPROFILE.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);
    init_fonts();

    create_profile_items();
    init_elements();

    if (ui_count > 0) {
        if (!is_network_connected()) {
            lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
            lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        }

        lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        list_nav_move(0, +1);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXNETPROFILE.NONE);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_save,
                    [MUX_INPUT_Y] = handle_remove,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
