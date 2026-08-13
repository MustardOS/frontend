#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxwebserv.h"

#define WEBSERV(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(WEBSERV_ELEMENTS) };
#undef WEBSERV

enum web_service {
    web_service_none = -1,
    web_service_sshd,
    web_service_sftpgo,
    web_service_ttyd,
    web_service_syncthing,
    web_service_tailscaled,
    web_service_count
};

enum web_field { web_field_none, web_field_port, web_field_secondary_port, web_field_username, web_field_password };

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

static enum web_service selected_service = web_service_none;
static enum web_field editing_field = web_field_none;
static int main_service_index;
static int fields_modified;
static int editing_enabled;
static char editing_port[6];
static char editing_secondary_port[6];
static char editing_username[33];
static char editing_password[129];

static mux_dialogue save_dlg;

static const char *service_key(const enum web_service service) {
    static const char *keys[web_service_count] = {"sshd", "sftpgo", "ttyd", "syncthing", "tailscaled"};
    return service >= 0 && service < web_service_count ? keys[service] : "";
}

static const char *service_title(const enum web_service service) {
    switch (service) {
        case web_service_sshd:
            return lang.muxwebserv.sshd;
        case web_service_sftpgo:
            return lang.muxwebserv.sftpgo;
        case web_service_ttyd:
            return lang.muxwebserv.ttyd;
        case web_service_syncthing:
            return lang.muxwebserv.syncthing;
        case web_service_tailscaled:
            return lang.muxwebserv.tailscaled;
        default:
            return lang.muxwebserv.title;
    }
}

static int16_t *service_enabled(const enum web_service service) {
    switch (service) {
        case web_service_sshd:
            return &config.web.sshd;
        case web_service_sftpgo:
            return &config.web.sftp_go;
        case web_service_ttyd:
            return &config.web.ttyd;
        case web_service_syncthing:
            return &config.web.syncthing;
        case web_service_tailscaled:
            return &config.web.tailscaled;
        default:
            return NULL;
    }
}

static char *service_port(const enum web_service service) {
    switch (service) {
        case web_service_sshd:
            return config.web.sshd_port;
        case web_service_sftpgo:
            return config.web.sftpgo_port;
        case web_service_ttyd:
            return config.web.ttyd_port;
        case web_service_syncthing:
            return config.web.syncthing_port;
        default:
            return NULL;
    }
}

static const char *service_default_port(const enum web_service service) {
    switch (service) {
        case web_service_sshd:
            return "22";
        case web_service_sftpgo:
            return "9090";
        case web_service_ttyd:
            return "8080";
        case web_service_syncthing:
            return "7070";
        default:
            return "";
    }
}

static char *service_secondary_port(const enum web_service service) {
    return service == web_service_sftpgo ? config.web.sftpgo_sftp_port : NULL;
}

static int service_has_login(const enum web_service service) {
    return service == web_service_ttyd;
}

static void set_row_visible(const int index, const int visible) {
    lv_obj_t *objects[] = {
        ui_objects[index], ui_objects_value[index], ui_objects_glyph[index], ui_objects_panel[index]
    };
    for (size_t i = 0; i < A_SIZE(objects); i++) {
        if (visible)
            lv_obj_clear_flag(objects[i], MU_OBJ_FLAG_HIDE_FLOAT);
        else
            lv_obj_add_flag(objects[i], MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void rebuild_groups(const int count, const int target_index) {
    ui_count_static = (unsigned int) count;
    current_item_index = 0;
    reset_ui_groups();

    for (int i = 0; i < ui_count_dynamic; i++)
        set_row_visible(i, i < count);

    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
    gen_step_movement(target_index >= 0 && target_index < count ? target_index : 0, +1, 2, 0, 0);
    nav_moved = 1;
}

static void set_row(const int index, const char *label, const char *glyph, const char *value, const char *help_key) {
    lv_label_set_text(ui_objects[index], label);
    lv_label_set_text(ui_objects_value[index], value);
    apply_theme_list_glyph(&theme, ui_objects_glyph[index], mux_module, glyph);
    lv_obj_set_user_data(ui_objects[index], (void *) help_key);
}

static void setup_service_nav(void) {
    nav_show_lr(0);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
}

static void show_main_view(void) {
    selected_service = web_service_none;
    fields_modified = 0;
    editing_field = web_field_none;

    lv_label_set_text(ui_lbl_title, lang.muxwebserv.title);
    set_row(0, lang.muxwebserv.sshd, "sshd", config.web.sshd ? lang.generic.enabled : lang.generic.disabled, "sshd");
    set_row(
        1, lang.muxwebserv.sftpgo, "sftpgo", config.web.sftp_go ? lang.generic.enabled : lang.generic.disabled, "sftpgo"
    );
    set_row(2, lang.muxwebserv.ttyd, "ttyd", config.web.ttyd ? lang.generic.enabled : lang.generic.disabled, "ttyd");
    set_row(
        3, lang.muxwebserv.syncthing, "syncthing", config.web.syncthing ? lang.generic.enabled : lang.generic.disabled,
        "syncthing"
    );
    set_row(
        4, lang.muxwebserv.tailscaled, "tailscaled",
        config.web.tailscaled ? lang.generic.enabled : lang.generic.disabled, "tailscaled"
    );

    setup_service_nav();
    rebuild_groups(web_service_count, main_service_index);
}

static void load_service_values(void) {
    const int16_t *enabled = service_enabled(selected_service);
    const char *port = service_port(selected_service);

    editing_enabled = enabled ? *enabled : 0;
    snprintf(editing_port, sizeof(editing_port), "%s", port && *port ? port : service_default_port(selected_service));
    snprintf(
        editing_secondary_port, sizeof(editing_secondary_port), "%s",
        config.web.sftpgo_sftp_port[0] ? config.web.sftpgo_sftp_port : "2022"
    );
    snprintf(editing_username, sizeof(editing_username), "%s", config.web.ttyd_user);
    snprintf(editing_password, sizeof(editing_password), "%s", config.web.ttyd_pass);
}

static int service_changed(void) {
    const int16_t *enabled = service_enabled(selected_service);
    if (enabled && editing_enabled != (*enabled != 0)) return 1;

    const char *port = service_port(selected_service);
    if (port && strcmp(editing_port, *port ? port : service_default_port(selected_service)) != 0) return 1;

    const char *secondary_port = service_secondary_port(selected_service);
    if (secondary_port && strcmp(editing_secondary_port, *secondary_port ? secondary_port : "2022") != 0) return 1;

    if (service_has_login(selected_service)
        && (strcmp(editing_username, config.web.ttyd_user) != 0 || strcmp(editing_password, config.web.ttyd_pass) != 0))
        return 1;

    return 0;
}

static void show_detail_view(const enum web_service service) {
    selected_service = service;
    main_service_index = service;
    fields_modified = 0;
    editing_field = web_field_none;
    load_service_values();

    lv_label_set_text(ui_lbl_title, service_title(service));
    set_row(
        0, lang.muxwebserv.service, "enabled", editing_enabled ? lang.generic.enabled : lang.generic.disabled, "enabled"
    );

    int count = 1;
    if (service_port(service)) {
        const int web_port = service == web_service_sftpgo || service == web_service_syncthing;
        set_row(
            count++, web_port ? lang.muxwebserv.web_port : lang.muxwebserv.port, "port", editing_port,
            web_port ? "web_port" : "port"
        );
    }
    if (service_secondary_port(service)) {
        set_row(count++, lang.muxwebserv.sftp_port, "port", editing_secondary_port, "sftp_port");
    }
    if (service_has_login(service)) {
        set_row(
            count++, lang.muxwebserv.username, "username",
            editing_username[0] ? editing_username : lang.muxwebserv.not_set, "username"
        );
        set_row(
            count++, lang.muxwebserv.password, "password", editing_password[0] ? "********" : lang.muxwebserv.not_set,
            "password"
        );
    }

    setup_service_nav();
    rebuild_groups(count, 0);
}

static int valid_port(const char *port) {
    if (!*port) return 0;

    for (const unsigned char *p = (const unsigned char *) port; *p; p++) {
        if (!isdigit(*p)) return 0;
    }

    errno = 0;
    char *end = NULL;
    const long value = strtol(port, &end, 10);
    return errno == 0 && end && *end == '\0' && value >= 1 && value <= 65535;
}

static int validate_service(void) {
    if (service_port(selected_service) && !valid_port(editing_port)) {
        play_sound(snd_error);
        toast_message(lang.muxwebserv.invalid_port, tst_wait_s);
        return 0;
    }

    if (service_secondary_port(selected_service) && !valid_port(editing_secondary_port)) {
        play_sound(snd_error);
        toast_message(lang.muxwebserv.invalid_port, tst_wait_s);
        return 0;
    }

    if (service_has_login(selected_service) && (editing_username[0] == '\0') != (editing_password[0] == '\0')) {
        play_sound(snd_error);
        toast_message(lang.muxwebserv.incomplete_login, tst_wait_s);
        return 0;
    }

    if (service_has_login(selected_service) && editing_username[0]) {
        for (const unsigned char *p = (const unsigned char *) editing_username; *p; p++) {
            if (!isalnum(*p) && *p != '_' && *p != '-') {
                play_sound(snd_error);
                toast_message(lang.muxwebserv.invalid_login, tst_wait_s);
                return 0;
            }
        }
        if (strchr(editing_password, ':')) {
            play_sound(snd_error);
            toast_message(lang.muxwebserv.invalid_login, tst_wait_s);
            return 0;
        }
    }

    return 1;
}

static int save_service(void) {
    if (!validate_service()) return 0;

    const char *key = service_key(selected_service);
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), CONF_CONFIG_PATH "web/%s", key);
    write_text_to_file_atomic(path, INT, editing_enabled);

    int16_t *enabled = service_enabled(selected_service);
    if (enabled) *enabled = editing_enabled;

    char *port = service_port(selected_service);
    if (port) {
        snprintf(path, sizeof(path), CONF_CONFIG_PATH "web/%s_port", key);
        write_text_to_file_atomic(path, CHAR, editing_port);
        snprintf(port, MAX_BUFFER_SIZE, "%s", editing_port);
    }

    char *secondary_port = service_secondary_port(selected_service);
    if (secondary_port) {
        write_text_to_file_atomic(CONF_CONFIG_PATH "web/sftpgo_sftp_port", CHAR, editing_secondary_port);
        snprintf(secondary_port, MAX_BUFFER_SIZE, "%s", editing_secondary_port);
    }

    if (service_has_login(selected_service)) {
        write_text_to_file_atomic(CONF_CONFIG_PATH "web/ttyd_user", CHAR, editing_username);
        write_text_to_file_atomic(CONF_CONFIG_PATH "web/ttyd_pass", CHAR, editing_password);
        chmod(CONF_CONFIG_PATH "web/ttyd_user", 0600);
        chmod(CONF_CONFIG_PATH "web/ttyd_pass", 0600);
        snprintf(config.web.ttyd_user, sizeof(config.web.ttyd_user), "%s", editing_username);
        snprintf(config.web.ttyd_pass, sizeof(config.web.ttyd_pass), "%s", editing_password);
    }

    toast_message(lang.generic.saving, tst_wait_m);

    const char *args[] = {OPT_PATH "script/web/service.sh", "apply", key, NULL};
    run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);

    refresh_config = 1;
    fields_modified = 0;
    return 1;
}

static void show_help(void) {
    if (selected_service == web_service_none) {
        const struct help_msg help_messages[] = {
            {"sshd", lang.muxwebserv.help.sshd},
            {"sftpgo", lang.muxwebserv.help.sftp_go},
            {"ttyd", lang.muxwebserv.help.ttyd},
            {"syncthing", lang.muxwebserv.help.syncthing},
            {"tailscaled", lang.muxwebserv.help.tailscaled}
        };
        gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
        return;
    }

    const struct help_msg help_messages[] = {
        {"enabled", lang.muxwebserv.help.service},   {"port", lang.muxwebserv.help.port},
        {"web_port", lang.muxwebserv.help.web_port}, {"sftp_port", lang.muxwebserv.help.sftp_port},
        {"username", lang.muxwebserv.help.username}, {"password", lang.muxwebserv.help.password}
    };
    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void cycle_enabled(void) {
    editing_enabled = !editing_enabled;
    lv_label_set_text(ui_objects_value[0], editing_enabled ? lang.generic.enabled : lang.generic.disabled);
    play_sound(snd_option);
    fields_modified = service_changed();
}

static void handle_keyboard_ok_press(void) {
    const char *text = lv_textarea_get_text(ui_txt_entry_webserv);

    switch (editing_field) {
        case web_field_port:
            snprintf(editing_port, sizeof(editing_port), "%s", text);
            lv_label_set_text(ui_objects_value[1], editing_port);
            break;
        case web_field_secondary_port:
            snprintf(editing_secondary_port, sizeof(editing_secondary_port), "%s", text);
            lv_label_set_text(ui_objects_value[2], editing_secondary_port);
            break;
        case web_field_username:
            snprintf(editing_username, sizeof(editing_username), "%s", text);
            lv_label_set_text(ui_objects_value[2], editing_username[0] ? editing_username : lang.muxwebserv.not_set);
            break;
        case web_field_password:
            snprintf(editing_password, sizeof(editing_password), "%s", text);
            lv_label_set_text(ui_objects_value[3], editing_password[0] ? "********" : lang.muxwebserv.not_set);
            break;
        default:
            break;
    }

    lv_obj_t *active = key_show == 2 ? num_entry : key_entry;
    reset_osk(active);
    lv_textarea_set_text(ui_txt_entry_webserv, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_webserv);
    key_show = 0;
    editing_field = web_field_none;
    fields_modified = service_changed();
}

static void handle_keyboard_press(void) {
    if (first_open)
        first_open = 0;
    else
        play_sound(snd_keypress);

    lv_obj_t *active = key_show == 2 ? num_entry : key_entry;
    const char *key = lv_btnmatrix_get_btn_text(active, key_curr);
    if (key && strcasecmp(key, OSK_DONE) == 0)
        handle_keyboard_ok_press();
    else
        lv_event_send(active, LV_EVENT_CLICKED, &key_curr);
}

static void open_editor(const enum web_field field) {
    editing_field = field;
    key_curr = 0;

    const int numeric = field == web_field_port || field == web_field_secondary_port;
    lv_textarea_set_password_mode(ui_txt_entry_webserv, field == web_field_password);
    lv_textarea_set_max_length(ui_txt_entry_webserv, numeric ? 5 : field == web_field_username ? 32 : 128);

    if (numeric) {
        lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(key_entry, LV_STATE_DISABLED);
        lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(num_entry, LV_STATE_DISABLED);
        key_show = 2;
        lv_textarea_set_text(ui_txt_entry_webserv, field == web_field_port ? editing_port : editing_secondary_port);
    } else {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);
        lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(num_entry, LV_STATE_DISABLED);
        key_show = 1;
        lv_textarea_set_text(ui_txt_entry_webserv, field == web_field_username ? editing_username : editing_password);
    }

    play_sound(snd_confirm);
    osk_show(ui_pnl_entry_webserv);
    osk_refresh_labels();
}

static void handle_confirm(void) {
    if (selected_service == web_service_none) {
        play_sound(snd_confirm);
        show_detail_view((enum web_service) current_item_index);
        return;
    }

    if (current_item_index == 0) {
        cycle_enabled();
        return;
    }

    if (service_port(selected_service) && current_item_index == 1) {
        open_editor(web_field_port);
        return;
    }

    if (service_secondary_port(selected_service) && current_item_index == 2) {
        open_editor(web_field_secondary_port);
        return;
    }

    if (service_has_login(selected_service)) {
        open_editor(current_item_index == 2 ? web_field_username : web_field_password);
    }
}

static void leave_detail(void) {
    fields_modified = service_changed();
    if (fields_modified && !config.settings.advanced.trust_modify) {
        play_sound(snd_confirm);
        dialogue_open(&save_dlg, &theme);
        return;
    }

    if (fields_modified && !save_service()) return;
    play_sound(snd_back);
    show_main_view();
}

static void handle_a(void) {
    if (dialogue_active(&save_dlg)) {
        const mux_unsaved_opt option = (mux_unsaved_opt) save_dlg.selected;
        dialogue_dismiss(&save_dlg);

        if (option == mux_unsaved_save && !save_service()) return;
        show_main_view();
        return;
    }

    if (msgbox_active || hold_call) return;
    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        dialogue_dismiss(&save_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_webserv);
        return;
    }

    if (selected_service != web_service_none) {
        leave_detail();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "service");
    mux_input_stop();
}

static void handle_b_hold(void) {
    if (!dialogue_active(&save_dlg) && key_show) key_backspace(ui_txt_entry_webserv);
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;
    if (dialogue_active(&save_dlg) || msgbox_active || hold_call || !key_show) return;

    close_osk(key_show == 2 ? num_entry : key_entry, ui_group, ui_txt_entry_webserv, ui_pnl_entry_webserv);
    editing_field = web_field_none;
}

static void handle_y(void) {
    if (!dialogue_active(&save_dlg) && !msgbox_active && !hold_call && key_show == 1) key_space(ui_txt_entry_webserv);
}

static void handle_select(void) {
    if (!dialogue_active(&save_dlg) && !msgbox_active && !hold_call && key_show) key_clear(ui_txt_entry_webserv);
}

static void handle_start(void) {
    if (!dialogue_active(&save_dlg) && !msgbox_active && !hold_call && key_show) handle_keyboard_ok_press();
}

static void handle_up(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }
    key_show ? key_up() : handle_list_nav_up();
}

static void handle_down(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }
    key_show ? key_down() : handle_list_nav_down();
}

static void handle_left(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }
    if (key_show) {
        key_left();
        return;
    }
    if (selected_service != web_service_none && current_item_index == 0) cycle_enabled();
}

static void handle_right(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }
    if (key_show) {
        key_right();
        return;
    }
    if (selected_service != web_service_none && current_item_index == 0) cycle_enabled();
}

static void handle_l1(void) {
    if (key_show == 1)
        key_swap_back();
    else if (!key_show)
        handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show == 1)
        key_swap();
    else if (!key_show)
        handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || key_show || hold_call
        || dialogue_active(&save_dlg))
        return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_entry_webserv,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, ui_pnl_message, NULL});
}

static void init_navigation_group(void) {
    INIT_VALUE_ITEM(-1, webserv, sshd, lang.muxwebserv.sshd, "sshd", "");
    INIT_VALUE_ITEM(-1, webserv, sftp_go, lang.muxwebserv.sftpgo, "sftpgo", "");
    INIT_VALUE_ITEM(-1, webserv, ttyd, lang.muxwebserv.ttyd, "ttyd", "");
    INIT_VALUE_ITEM(-1, webserv, syncthing, lang.muxwebserv.syncthing, "syncthing", "");
    INIT_VALUE_ITEM(-1, webserv, tailscaled, lang.muxwebserv.tailscaled, "tailscaled", "");

    show_main_view();
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();
    overlay_display();
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) handle_keyboard_ok_press();
    ev.code == KEY_ESC &&ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txt_entry_webserv);
}

int muxwebserv_main(void) {
    selected_service = web_service_none;
    editing_field = web_field_none;
    main_service_index = 0;
    fields_modified = 0;
    save_dlg.active = 0;

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxwebserv.title);
    init_muxwebserv(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_osk(ui_pnl_entry_webserv, ui_txt_entry_webserv, 1, 0, 128);

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);

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
                [mux_input_select] = handle_select,
                [mux_input_start] = handle_start,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_up] = handle_up,
            [mux_input_dpad_down] = handle_down,
            [mux_input_dpad_left] = handle_left,
            [mux_input_dpad_right] = handle_right,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    orientation_introduce(mux_module, lang.muxwebserv.title, lang.muxwebserv.overview);

    mux_input_task(&input_opts);
    return 0;
}
