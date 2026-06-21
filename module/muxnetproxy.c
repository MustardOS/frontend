#include "muxshare.h"
#include "ui/ui_muxnetproxy.h"

#define PROXY(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(PROXY_ELEMENTS)
};
#undef PROXY

static int fields_modified = 0;
static int test_running = 0;

static mux_dialogue save_dlg;
static int save_dlg_active = 0;

static const char *proxy_schemes[] = {"http://", "https://", "socks5://"};

#define PROXY_SCHEME_COUNT 3
#define PROXY_TEST_RESULT  "/run/muos/proxy_test.txt"

static void show_help(void) {
    struct help_msg help_messages[] = {
#define PROXY(NAME, ENUM, UDATA) { UDATA, lang.MUXNETPROXY.HELP.ENUM },
            PROXY_ELEMENTS
#undef PROXY
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int current_type_index(void) {
    const char *label = lv_label_get_text(ui_lblTypeValue_proxy);
    if (!label) return 0;
    if (strcasecmp(label, lang.MUXNETPROXY.HTTPS) == 0) return 1;
    if (strcasecmp(label, lang.MUXNETPROXY.SOCKS5) == 0) return 2;
    return 0;
}

static const char *type_label_for(int idx) {
    switch (idx) {
        case 1:
            return lang.MUXNETPROXY.HTTPS;
        case 2:
            return lang.MUXNETPROXY.SOCKS5;
        default:
            return lang.MUXNETPROXY.HTTP;
    }
}

static int current_enabled(void) {
    const char *curr = lv_label_get_text(ui_lblEnabledValue_proxy);
    return (curr && strcasecmp(curr, lang.GENERIC.ENABLED) == 0) ? 1 : 0;
}

static void cycle_enabled(int dir) {
    int enabled = (current_enabled() + dir + 2) % 2;
    lv_label_set_text(ui_lblEnabledValue_proxy,
                      enabled ? lang.GENERIC.ENABLED : lang.GENERIC.DISABLED);
    play_sound(SND_OPTION);
    fields_modified = 1;
}

static void cycle_type(int dir) {
    int idx = current_type_index();
    idx = (idx + dir + PROXY_SCHEME_COUNT) % PROXY_SCHEME_COUNT;
    lv_label_set_text(ui_lblTypeValue_proxy, type_label_for(idx));
    play_sound(SND_OPTION);
    fields_modified = 1;
}

static void write_proxy_vars(FILE *f, const char *proxy_url, const char *noproxy, int use_export) {
    const char *pfx = use_export ? "export " : "";
    fprintf(f, "%sHTTP_PROXY=%s\n", pfx, proxy_url);
    fprintf(f, "%sHTTPS_PROXY=%s\n", pfx, proxy_url);
    fprintf(f, "%sALL_PROXY=%s\n", pfx, proxy_url);
    fprintf(f, "%shttp_proxy=%s\n", pfx, proxy_url);
    fprintf(f, "%shttps_proxy=%s\n", pfx, proxy_url);
    fprintf(f, "%sall_proxy=%s\n", pfx, proxy_url);
    if (noproxy && *noproxy) {
        fprintf(f, "%sNO_PROXY=%s\n", pfx, noproxy);
        fprintf(f, "%sno_proxy=%s\n", pfx, noproxy);
    }
}

static void write_environment_file(void) {
    const char *server = lv_label_get_text(ui_lblServerValue_proxy);
    const char *noproxy = lv_label_get_text(ui_lblNoProxyValue_proxy);
    int active = current_enabled() && server && *server;

    char proxy_url[MAX_BUFFER_SIZE];
    if (active) {
        snprintf(proxy_url, sizeof(proxy_url), "%s%s",
                 proxy_schemes[current_type_index()], server);
    }

    FILE *f = fopen("/etc/environment", "w");
    if (f) {
        if (active) write_proxy_vars(f, proxy_url, noproxy, 0);
        fclose(f);
    }

    if (active) {
        mkdir("/etc/profile.d", 0755);
        FILE *pf = fopen("/etc/profile.d/proxy.sh", "w");
        if (pf) {
            write_proxy_vars(pf, proxy_url, noproxy, 1);
            fclose(pf);
        }
    } else {
        remove("/etc/profile.d/proxy.sh");
    }
}

static void save_proxy_options(void) {
    const char *server = lv_label_get_text(ui_lblServerValue_proxy);
    const char *noproxy = lv_label_get_text(ui_lblNoProxyValue_proxy);

    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/network/proxy_enabled", INT, current_enabled());
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/network/proxy_type", INT, current_type_index());
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/network/proxy_server", CHAR, server ? server : "");
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/network/proxy_noproxy", CHAR, noproxy ? noproxy : "");

    write_environment_file();

    refresh_config = 1;
    fields_modified = 0;

    toast_message(lang.MUXNETPROXY.SAVED, LONG);
}

static void restore_proxy_values(void) {
    lv_label_set_text(ui_lblEnabledValue_proxy, config.SETTINGS.NETWORK.PROXY_ENABLED ? lang.GENERIC.ENABLED : lang.GENERIC.DISABLED);
    lv_label_set_text(ui_lblTypeValue_proxy, type_label_for(config.SETTINGS.NETWORK.PROXY_TYPE));
    lv_label_set_text(ui_lblServerValue_proxy, config.SETTINGS.NETWORK.PROXY_SERVER);

    const char *noproxy = config.SETTINGS.NETWORK.PROXY_NOPROXY;
    lv_label_set_text(ui_lblNoProxyValue_proxy, (noproxy && *noproxy) ? noproxy : "localhost,127.0.0.1,::1");

    lv_label_set_text(ui_lblTestValue_proxy, "");
}

static void run_proxy_test(void) {
    const char *server = lv_label_get_text(ui_lblServerValue_proxy);

    if (!server || !*server) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNETPROXY.NO_SERVER, SHORT);
        return;
    }

    static char proxy_url[MAX_BUFFER_SIZE];
    snprintf(proxy_url, sizeof(proxy_url), "%s%s",
             proxy_schemes[current_type_index()], server);

    remove(PROXY_TEST_RESULT);

    test_running = 1;
    lv_label_set_text(ui_lblTestValue_proxy, lang.MUXNETPROXY.TESTING);

    const char *test_args[] = {
            "curl", "-x", proxy_url,
            "-o", "/dev/null", "-s",
            "-w", "%{http_code}",
            "-m", "5",
            "http://1.1.1.1",
            NULL
    };

    run_exec(test_args, A_SIZE(test_args), 1, 0, PROXY_TEST_RESULT, NULL);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lblServer_proxy) {
        lv_label_set_text(ui_lblServerValue_proxy, lv_textarea_get_text(ui_txtEntry_proxy));
    } else if (e_focused == ui_lblNoProxy_proxy) {
        lv_label_set_text(ui_lblNoProxyValue_proxy, lv_textarea_get_text(ui_txtEntry_proxy));
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_proxy, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnlEntry_proxy);
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

static void handle_confirm(void) {
    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lblEnabled_proxy) {
        cycle_enabled(+1);
        return;
    }

    if (e_focused == ui_lblType_proxy) {
        cycle_type(+1);
        return;
    }

    if (e_focused == ui_lblTest_proxy) {
        if (test_running) return;
        play_sound(SND_CONFIRM);
        run_proxy_test();
        return;
    }

    play_sound(SND_CONFIRM);
    key_curr = 0;
    lv_textarea_set_password_mode(ui_txtEntry_proxy, 0);

    lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(key_entry, LV_STATE_DISABLED);
    lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(num_entry, LV_STATE_DISABLED);

    key_show = 1;
    osk_show(ui_pnlEntry_proxy);
    osk_refresh_labels();

    lv_textarea_set_text(ui_txtEntry_proxy, lv_label_get_text(lv_group_get_focused(ui_group_value)));
}

static void handle_back(void) {
    if (fields_modified) {
        play_sound(SND_CONFIRM);
        save_dlg_active = 1;
        save_dlg.selected = 0;
        dialogue_show(&save_dlg);
        dialogue_refresh(&save_dlg, &theme);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_proxy");
    mux_input_stop();
}

static void handle_a(void) {
    if (save_dlg_active) {
        mux_confirm_opt opt = (mux_confirm_opt) save_dlg.selected;
        save_dlg_active = 0;
        dialogue_hide(&save_dlg);

        if (opt == MUX_CONFIRM_YEP) save_proxy_options();

        play_sound(SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "net_proxy");
        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_b(void) {
    if (hold_call) return;

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
        key_backspace(ui_txtEntry_proxy);
        return;
    }

    handle_back();
}

static void handle_b_hold(void) {
    if (save_dlg_active) return;
    if (key_show) key_backspace(ui_txtEntry_proxy);
}

static void handle_x(void) {
    if (save_dlg_active || msgbox_active || hold_call) return;

    if (key_show) {
        close_osk(lv_obj_has_state(key_entry, LV_STATE_DISABLED) ? num_entry : key_entry, ui_group, ui_txtEntry_proxy, ui_pnlEntry_proxy);
        return;
    }

    if (!fields_modified) return;
    play_sound(SND_CONFIRM);
    save_proxy_options();
}

static void handle_y(void) {
    if (save_dlg_active || msgbox_active || hold_call) return;

    if (key_show == 1) {
        key_space(ui_txtEntry_proxy);
        return;
    }
}

static void handle_up(void) {
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
    if (save_dlg_active) return;
    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
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
    if (save_dlg_active) return;
    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (save_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (!key_show) {
        struct _lv_obj_t *focused = lv_group_get_focused(ui_group);
        if (focused == ui_lblEnabled_proxy) {
            cycle_enabled(-1);
            return;
        }
        if (focused == ui_lblType_proxy) {
            cycle_type(-1);
            return;
        }
    }

    if (key_show) key_left();
}

static void handle_right(void) {
    if (save_dlg_active) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (!key_show) {
        struct _lv_obj_t *focused = lv_group_get_focused(ui_group);
        if (focused == ui_lblEnabled_proxy) {
            cycle_enabled(+1);
            return;
        }
        if (focused == ui_lblType_proxy) {
            cycle_type(+1);
            return;
        }
    }

    if (key_show) key_right();
}

static void handle_left_hold(void) {
    if (save_dlg_active) return;

    if (!key_show) {
        struct _lv_obj_t *focused = lv_group_get_focused(ui_group);
        if (focused == ui_lblEnabled_proxy) {
            cycle_enabled(-1);
            return;
        }
        if (focused == ui_lblType_proxy) {
            cycle_type(-1);
            return;
        }
    }

    if (key_show) key_left();
}

static void handle_right_hold(void) {
    if (save_dlg_active) return;

    if (!key_show) {
        struct _lv_obj_t *focused = lv_group_get_focused(ui_group);
        if (focused == ui_lblEnabled_proxy) {
            cycle_enabled(+1);
            return;
        }
        if (focused == ui_lblType_proxy) {
            cycle_type(+1);
            return;
        }
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

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call || save_dlg_active) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlEntry_proxy,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, proxy, Enabled, lang.MUXNETPROXY.ENABLED, "enabled", "");
    INIT_VALUE_ITEM(-1, proxy, Type, lang.MUXNETPROXY.TYPE, "type", "");
    INIT_VALUE_ITEM(-1, proxy, Server, lang.MUXNETPROXY.SERVER, "server", "");
    INIT_VALUE_ITEM(-1, proxy, NoProxy, lang.MUXNETPROXY.NOPROXY, "noproxy", "");
    INIT_VALUE_ITEM(-1, proxy, Test, lang.MUXNETPROXY.TEST, "test", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(0, +1, 1, 0, 1);
    nav_moved = 1;
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.SAVE,   0},
            {NULL, NULL,                            0}
    });

#define PROXY(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_proxy, UDATA);
    PROXY_ELEMENTS
#undef PROXY

    overlay_display();
}

static void ui_refresh_task() {
    if (test_running && file_exist(PROXY_TEST_RESULT)) {
        char *result = read_line_char_from(PROXY_TEST_RESULT, 1);
        if (result && *result) {
            test_running = 0;

            char msg[MAX_BUFFER_SIZE];
            int code = safe_atoi(result, 0);
            if (code >= 200 && code < 400) {
                snprintf(msg, sizeof(msg), "%s (HTTP %s)", lang.MUXNETPROXY.TEST_OK, result);
            } else if (code == 0) {
                snprintf(msg, sizeof(msg), "%s", lang.MUXNETPROXY.TEST_FAIL);
            } else {
                snprintf(msg, sizeof(msg), "%s (HTTP %s)", lang.MUXNETPROXY.TEST_FAIL, result);
            }

            lv_label_set_text(ui_lblTestValue_proxy, msg);
        }
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
    ev.code == KEY_ESC && ev.value == 1 ? handle_b() : process_key_event(&ev, ui_txtEntry_proxy);
}

int muxnetproxy_main(void) {
    fields_modified = 0;
    test_running = 0;
    save_dlg_active = 0;

    remove(PROXY_TEST_RESULT);

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETPROXY.TITLE);
    init_muxnetproxy(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_proxy_values();

    init_osk(ui_pnlEntry_proxy, ui_txtEntry_proxy, 1, 0, OSK_MAX);

    dialogue_init_confirm(&save_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_X]          = handle_x,
                    [MUX_INPUT_Y]          = handle_y,
                    [MUX_INPUT_DPAD_UP]    = handle_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_down,
                    [MUX_INPUT_DPAD_LEFT]  = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1]         = handle_l1,
                    [MUX_INPUT_R1]         = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_B]          = handle_b_hold,
                    [MUX_INPUT_DPAD_UP]    = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN]  = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT]  = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1]         = handle_l1,
                    [MUX_INPUT_R1]         = handle_r1,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);

    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
