#include "muxshare.h"
#include "ui/ui_muxtweakgen.h"

static int save_mode = 0;
static mux_dialogue save_dlg;
static char pending_submenu[64] = "";

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
    pending_submenu[0] = '\0';
}

static int warn_mode = 0;
static mux_dialogue warn_dlg;
static char warn_pending[64] = "";

static void show_warn_dialog(const char *target) {
    warn_mode = 1;
    warn_dlg.selected = 1;
    snprintf(warn_pending, sizeof(warn_pending), "%s", target);

    if (warn_dlg.description_label) {
        const char *desc = strcmp(target, "danger") == 0 ? lang.muxdanger.warn : lang.muxtweakgen.warn;
        lv_label_set_text(warn_dlg.description_label, desc);
    }

    dialogue_show(&warn_dlg);
    dialogue_refresh(&warn_dlg, &theme);
}

static void hide_warn_dialog(void) {
    warn_mode = 0;
    dialogue_hide(&warn_dlg);
    warn_pending[0] = '\0';
}

#define TWEAKGEN(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(TWEAKGEN_ELEMENTS) };
#undef TWEAKGEN

#define TWEAKGEN(NAME, UDATA) static int NAME##_original;
TWEAKGEN_ELEMENTS
#undef TWEAKGEN

static int any_tweakgen_modified(void) {
#define TWEAKGEN(NAME, UDATA)                                                                                          \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_tweakgen) != NAME##_original) return 1;
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN
    return 0;
}

static int audio_overdrive = 100;
static char **audio_sinks = NULL;
static int audio_sink_count = 0;

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define TWEAKGEN(NAME, UDATA) {UDATA, lang.muxtweakgen.help.NAME},
        TWEAKGEN_ELEMENTS
#undef TWEAKGEN
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_audio_limits(void) {
    audio_overdrive = config.settings.advanced.overdrive ? 200 : 100;
}

static int visible_hdmi(void) {
    return !lv_obj_has_flag(ui_pnl_hdmi_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static int visible_audiosink(void) {
    return !lv_obj_has_flag(ui_pnl_audio_sink_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static int visible_rgb(void) {
    return !lv_obj_has_flag(ui_pnl_rgb_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static void init_dropdown_settings(void) {
#define TWEAKGEN(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_tweakgen);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN
}

static void restore_tweak_options(void) {
    lv_dropdown_set_selected(
        ui_dro_brightness_tweakgen, int_to_pct(config.settings.general.brightness, 2, device.screen.bright)
    );
    lv_dropdown_set_selected(
        ui_dro_volume_tweakgen,
        clamp_range(config.settings.general.volume, 0, lv_dropdown_get_option_cnt(ui_dro_volume_tweakgen) - 1)
    );
    lv_dropdown_set_selected(ui_dro_hk_dpad_tweakgen, device.board.has_stick > 0 ? 0 : config.settings.general.hkdpad);
    lv_dropdown_set_selected(ui_dro_hk_shot_tweakgen, config.settings.general.hkshot);

    if (audio_sink_count > 0)
        lv_dropdown_set_selected(
            ui_dro_audio_sink_tweakgen, clamp_range(config.settings.general.audiosink, 0, audio_sink_count - 1)
        );

    lv_dropdown_set_selected(
        ui_dro_startup_tweakgen, strcasecmp(config.settings.general.startup, "explore") == 0      ? 1
                                 : strcasecmp(config.settings.general.startup, "collection") == 0 ? 2
                                 : strcasecmp(config.settings.general.startup, "history") == 0    ? 3
                                 : strcasecmp(config.settings.general.startup, "last") == 0       ? 4
                                 : strcasecmp(config.settings.general.startup, "resume") == 0     ? 5
                                                                                                  : 0
    );
}

static void save_tweak_options(void) {
    int is_modified = 0;

    const char *startup_options[] = {"launcher", "explore", "collection", "history", "last", "resume"};

    CHECK_AND_SAVE_VAL(tweakgen, startup, "settings/general/startup", CHAR, startup_options);
    CHECK_AND_SAVE_STD(tweakgen, hk_dpad, "settings/hotkey/dpad_toggle", INT, 0);
    CHECK_AND_SAVE_STD(tweakgen, hk_shot, "settings/hotkey/screenshot", INT, 0);

    if (audio_sink_count > 0) {
        const int sink_mod = lv_dropdown_get_selected(ui_dro_audio_sink_tweakgen);

        if (sink_mod != audio_sink_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/general/audiosink", "w", INT, sink_mod);

            char idx_str[8];
            snprintf(idx_str, sizeof(idx_str), "%d", sink_mod);

            const char *sink_args[] = {OPT_PATH "script/mux/audio_sink.sh", "set", idx_str, NULL};
            run_exec(sink_args, A_SIZE(sink_args), 1, 0, NULL, NULL);
        }
    }

    const int bright_mod = pct_to_int(lv_dropdown_get_selected(ui_dro_brightness_tweakgen), 2, device.screen.bright);
    if (lv_dropdown_get_selected(ui_dro_brightness_tweakgen) != brightness_original)
        set_setting_value("bright", bright_mod, 0);

    if (!hdmi_mode) {
        const int volume_mod = lv_dropdown_get_selected(ui_dro_volume_tweakgen);
        if (volume_mod != volume_original) set_setting_value("audio", volume_mod, 0);
    }

    if (is_modified > 0) run_tweak_script(lang.generic.saving);
}

static char **load_combos(const char *filename, int *count) {
    int line_count = 0;
    char **combos = str_parse_file(filename, &line_count, parse_lines);
    if (!combos || line_count == 0) return NULL;

    char **hk_lines = calloc(line_count, sizeof(char *));
    if (!hk_lines) {
        for (int i = 0; i < line_count; i++)
            free(combos[i]);
        free(combos);
        return NULL;
    }

    for (int i = 0; i < line_count; i++) {
        const char *combo = combos[i];

        char *open_bracket = strchr(combo, '[');
        char *close_bracket = strrchr(combo, ']');

        if (!open_bracket || !close_bracket || open_bracket >= close_bracket) {
            hk_lines[i] = NULL;
            continue;
        }

        *close_bracket = '\0';
        const char *scan_pos = open_bracket + 1;

        char buffer[128] = {0};
        char *write_pos = buffer;
        int is_first = 1;

        while (*scan_pos) {
            if (*scan_pos == '"') {
                const char *entry_start = ++scan_pos;
                while (*scan_pos && *scan_pos != '"')
                    scan_pos++;

                const size_t entry_len = scan_pos - entry_start;
                if (entry_len > 0) {
                    if (!is_first) *write_pos++ = '+';

                    for (size_t j = 0; j < entry_len; j++) {
                        const char c = entry_start[j];
                        *write_pos++ = c == '_' ? ' ' : c;
                    }

                    is_first = 0;
                }
            }

            if (*scan_pos) scan_pos++;
        }

        *write_pos = '\0';
        hk_lines[i] = strdup(buffer);
    }

    for (int i = 0; i < line_count; i++)
        free(combos[i]);
    free(combos);

    *count = line_count;
    return hk_lines;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *startup_options[] = {lang.muxtweakgen.startup.menu,       lang.muxtweakgen.startup.explore,
                               lang.muxtweakgen.startup.collection, lang.muxtweakgen.startup.history,
                               lang.muxtweakgen.startup.last,       lang.muxtweakgen.startup.resume};

    int hk_combo_count = 0;
    char **hk_combos = NULL;

    const char *combo_path = NULL;
    if (str_startswith(device.board.name, "rg")) {
        combo_path = OPT_PATH "share/hotkey/rg.ini";
    } else if (str_startswith(device.board.name, "tui")) {
        combo_path = OPT_PATH "share/hotkey/tui.ini";
    }

    if (combo_path) hk_combos = load_combos(combo_path, &hk_combo_count);

    const char *sink_args[] = {OPT_PATH "script/mux/audio_sink.sh", "list", NULL};
    run_exec(sink_args, A_SIZE(sink_args), 0, 1, NULL, NULL);
    audio_sinks = str_parse_file("/run/muos/audio_sinks", &audio_sink_count, parse_lines);

    INIT_OPTION_ITEM(-1, tweakgen, rtc, lang.muxtweakgen.rtc, "clock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, hdmi, lang.muxtweakgen.hdmi, "hdmi", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, rgb, lang.muxtweakgen.rgb, "rgb", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, input_remap, lang.muxtweakgen.inputremap, "inputremap", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, advanced, lang.muxtweakgen.advanced, "advanced", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, pass_code, lang.muxtweakgen.passcode, "lock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, display_temp, lang.muxtweakgen.displaytemp, "displaytemp", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, brightness, lang.muxtweakgen.brightness, "brightness", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, volume, lang.muxtweakgen.volume, "volume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, audio_sink, lang.muxtweakgen.audiosink, "audiosink", audio_sinks, audio_sink_count);
    INIT_OPTION_ITEM(-1, tweakgen, hk_dpad, lang.muxtweakgen.hkdpad, "hkdpad", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, hk_shot, lang.muxtweakgen.hkshot, "hkshot", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, startup, lang.muxtweakgen.startup.title, "startup", startup_options, 6);

    char *bright_pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_dro_brightness_tweakgen, bright_pct_values);
    free(bright_pct_values);

    char *volume_pct_values = generate_number_string(0, audio_overdrive, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_dro_volume_tweakgen, volume_pct_values);
    free(volume_pct_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!device.board.has_hdmi) HIDE_OPTION_ITEM(tweakgen, hdmi);
    if (!device.board.has_rgb) HIDE_OPTION_ITEM(tweakgen, rgb);
    if (device.board.has_stick > 0) HIDE_OPTION_ITEM(tweakgen, hk_dpad);
    if (!audio_sink_count) HIDE_OPTION_ITEM(tweakgen, audio_sink);

    if (!hk_combo_count) {
        HIDE_OPTION_ITEM(tweakgen, hk_dpad);
        HIDE_OPTION_ITEM(tweakgen, hk_shot);
    }

    if (hdmi_mode) {
        HIDE_OPTION_ITEM(tweakgen, display_temp);
        HIDE_OPTION_ITEM(tweakgen, brightness);
        HIDE_OPTION_ITEM(tweakgen, volume);
    }

    list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
}

static void nav_show_a(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_a, text);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_lr(const int show) {
    if (show) {
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    const int is_module = e_focused == ui_lbl_hdmi_tweakgen || e_focused == ui_lbl_rtc_tweakgen
                          || e_focused == ui_lbl_advanced_tweakgen || e_focused == ui_lbl_rgb_tweakgen
                          || e_focused == ui_lbl_pass_code_tweakgen || e_focused == ui_lbl_input_remap_tweakgen
                          || e_focused == ui_lbl_display_temp_tweakgen;
    const int is_set_opt = e_focused == ui_lbl_brightness_tweakgen || e_focused == ui_lbl_volume_tweakgen;

    if (is_module) {
        nav_show_a(1, lang.generic.select);
        nav_show_lr(0);
    } else if (is_set_opt) {
        nav_show_a(1, lang.generic.set);
        nav_show_lr(1);
    } else {
        nav_show_a(0, lang.generic.select);
        nav_show_lr(1);
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

#define HANDLE_TWEAK_OPT(TYPE, TOAST, VALUE, SCRIPT, OFFSET)                                                           \
    do {                                                                                                               \
        if (e_focused == ui_lbl_##TYPE##_tweakgen) {                                                                   \
            int v = (VALUE);                                                                                           \
            if (v != TYPE##_original) {                                                                                \
                toast_message(TOAST, tst_wait_s);                                                                      \
                set_setting_value(SCRIPT, v, (OFFSET));                                                                \
                TYPE##_original = (v - OFFSET);                                                                        \
            }                                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

static void update_option_values(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_brightness_tweakgen) {
        const int idx = lv_dropdown_get_selected(ui_dro_brightness_tweakgen);
        if (idx != brightness_original) {
            toast_message(lang.muxtweakgen.brightness_set, tst_wait_s);
            set_setting_value("bright", pct_to_int(idx, 2, device.screen.bright), 0);
            brightness_original = idx;
        }
        return;
    }

    HANDLE_TWEAK_OPT(volume, lang.muxtweakgen.volume_set, lv_dropdown_get_selected(ui_dro_volume_tweakgen), "audio", 0);
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    if (warn_mode) {
        if (swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    if (warn_mode) {
        if (swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static int get_multi_count(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_brightness_tweakgen) {
        return config.settings.advanced.inc_bright;
    }
    if (e_focused == ui_lbl_volume_tweakgen) {
        return config.settings.advanced.inc_volume;
    }

    return 0;
}

static void handle_option_prev_multi(void) {
    if (msgbox_active || block_input || save_mode || warn_mode) return;

    move_option(lv_group_get_focused(ui_group_value), -get_multi_count());
}

static void handle_option_next_multi(void) {
    if (msgbox_active || block_input || save_mode || warn_mode) return;

    move_option(lv_group_get_focused(ui_group_value), +get_multi_count());
}

static void handle_a(void) {
    if (msgbox_active || block_input || hold_call) return;

    if (warn_mode) {
        const int idx = warn_dlg.selected;
        char target[64];
        snprintf(target, sizeof(target), "%s", warn_pending);
        hide_warn_dialog();

        if (idx == 0) {
            if (strcmp(target, "danger") == 0) {
                char c_path[MAX_BUFFER_SIZE];
                snprintf(c_path, sizeof(c_path), CONF_CONFIG_PATH "count/warn_danger");

                create_directories(c_path, 1);

                write_text_to_file(c_path, "w", INT, read_line_int_from(c_path, 1) + 1);
                play_sound(snd_confirm);

                load_mux("danger");
                mux_input_stop();
            } else if (strcmp(target, "tweakadv") == 0) {
                char c_path[MAX_BUFFER_SIZE];
                snprintf(c_path, sizeof(c_path), CONF_CONFIG_PATH "count/warn_tweakadv");

                create_directories(c_path, 1);
                write_text_to_file(c_path, "w", INT, read_line_int_from(c_path, 1) + 1);

                if (!config.settings.advanced.trust_modify && any_tweakgen_modified()) {
                    snprintf(pending_submenu, sizeof(pending_submenu), "%s", "tweakadv");
                    show_save_dialog();
                } else {
                    play_sound(snd_confirm);
                    save_tweak_options();

                    load_mux("tweakadv");
                    mux_input_stop();
                }
            }
        }
        return;
    }

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        char submenu[64];
        snprintf(submenu, sizeof(submenu), "%s", pending_submenu);
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_tweak_options();

        if (submenu[0]) {
            play_sound(snd_confirm);
            load_mux(submenu);
        } else {
            play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
        }

        mux_input_stop();
        return;
    }

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_toggle = 0,
        menu_option,
        menu_clock,
        menu_hdmi,
        menu_rgb,
        menu_remap,
        menu_advanced,
        menu_passcode,
        menu_display,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"rtc", &kiosk.datetime.clock, menu_clock, NULL},
        {"hdmi", &kiosk.setting.hdmi, menu_hdmi, visible_hdmi},
        {"rgb", &kiosk.setting.rgb, menu_rgb, visible_rgb},
        {"remap", &kiosk_pass, menu_remap, NULL},
        {"tweakadv", &kiosk.setting.advanced, menu_advanced, NULL},
        {"passcfg", &kiosk_pass, menu_passcode, NULL},
        {"distemp", &kiosk_pass, menu_display, NULL}, // Display Temperature
        {NULL, &kiosk_pass, menu_option, NULL},       // Brightness
        {NULL, &kiosk_pass, menu_option, NULL},       // Volume
        {NULL, &kiosk_pass, menu_toggle, visible_audiosink},
        {NULL, &kiosk_pass, menu_toggle, NULL}, // Hotkey DPAD
        {NULL, &kiosk_pass, menu_toggle, NULL}, // Hotkey Screenshot
        {NULL, &kiosk_pass, menu_toggle, NULL}, // Startup Mode
    };

    const menu_entry *visible_entries[ui_count_dynamic];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    switch (entry->action) {
        case menu_clock:
        case menu_hdmi:
        case menu_rgb:
        case menu_remap:
        case menu_passcode:
        case menu_display:
            if (is_ksk(*entry->kiosk_flag)) {
                kiosk_denied();
                return;
            }

            if (!config.settings.advanced.trust_modify && any_tweakgen_modified()) {
                snprintf(pending_submenu, sizeof(pending_submenu), "%s", entry->mux_name);
                show_save_dialog();
                return;
            }

            play_sound(snd_confirm);
            save_tweak_options();
            load_mux(entry->mux_name);

            mux_input_stop();
            break;
        case menu_advanced:
            if (is_ksk(*entry->kiosk_flag)) {
                kiosk_denied();
                return;
            }
            show_warn_dialog("tweakadv");
            break;
        case menu_option:
            update_option_values();
            break;
        case menu_toggle:
            handle_option_next();
            break;
        default:
            break;
    }
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (warn_mode) {
        hide_warn_dialog();
        return;
    }

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_tweakgen_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);
    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (warn_mode) {
        if (!swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (warn_mode) {
        if (!swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode || warn_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode || warn_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || block_input || hold_call || save_mode
        || warn_mode)
        return;

    play_sound(snd_info_open);
    show_help();
}

static void launch_danger(void) {
    if (msgbox_active || hold_call || save_mode || warn_mode) return;

    if (lv_group_get_focused(ui_group) == ui_lbl_advanced_tweakgen) show_warn_dialog("danger");
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    check_focus();

#define TWEAKGEN(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_tweakgen, UDATA);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN

    overlay_display();
}

int muxtweakgen_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtweakgen.title);
    init_muxtweakgen(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_audio_limits();

    init_navigation_group();

    restore_tweak_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
    );
    dialogue_init_warn(&warn_dlg, &theme, ui_screen, lang.muxtweakgen.warn, lang.generic.select, lang.generic.back);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_l2] = handle_option_prev_multi,
                [mux_input_r1] = handle_list_nav_page_down,
                [mux_input_r2] = handle_option_next_multi,
            },
        .release_handler =
            {
                [mux_input_l2] = hold_call_release,
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_x] = launch_danger,
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_l2] = hold_call_set,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
