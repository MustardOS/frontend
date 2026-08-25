#include "muxshare.h"
#include "../common/ui/list_frame.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxtweakgen.h"

static mux_dialogue save_dlg;
static char pending_submenu[64] = "";

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_dlg);
    pending_submenu[0] = '\0';
}

#define TWEAKGEN(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(TWEAKGEN_ELEMENTS) };

enum {
    tweakgen_off_options = 0,
    tweakgen_len_options = E_SIZE(TWEAKGEN_OPTIONS_ELEMENTS),
    tweakgen_off_submenu = tweakgen_off_options + tweakgen_len_options,
    tweakgen_len_submenu = E_SIZE(TWEAKGEN_SUBMENU_ELEMENTS),
};
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

#define AUDIO_SINK_LIST RUN_PATH "audio_sinks"

static int audio_overdrive = 100;
static char **audio_sinks = NULL;
static int audio_sink_count = 0;
static int audio_sink_refresh_ticks = 0;
static long audio_sink_stamp = 0;
static char audio_sink_name_last[MAX_BUFFER_SIZE] = {0};
static char audio_sink_name_seen[MAX_BUFFER_SIZE] = {0};

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

static void restore_sink_volume(const int sink_index, const int save_outgoing) {
    if (save_outgoing && audio_sink_name_last[0]) {
        char *target = audio_sink_name(sink_index);

        if (!target || strcmp(target, audio_sink_name_last) != 0) {
            audio_sink_volume_store_name(audio_sink_name_last, current_volume);
        }

        free(target);
    }

    const int level = audio_sink_volume_load(sink_index, config.settings.general.volume);

    current_volume = level;
    config.settings.general.volume = (int16_t) level;

    lv_dropdown_set_selected(
        ui_dro_volume_tweakgen, clamp_range(level, 0, lv_dropdown_get_option_cnt(ui_dro_volume_tweakgen) - 1)
    );

    volume_original = lv_dropdown_get_selected(ui_dro_volume_tweakgen);

    char *landed = audio_sink_name(sink_index);
    snprintf(audio_sink_name_last, sizeof(audio_sink_name_last), "%s", landed ? landed : "");
    free(landed);

    LOG_INFO(
        mux_module, "Sink %d '%s' level %d applied, row now shows %d", sink_index, audio_sink_name_last, level,
        lv_dropdown_get_selected(ui_dro_volume_tweakgen)
    );
}

static int visible_audiosink(void) {
    return !lv_obj_has_flag(ui_pnl_audio_sink_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static void reload_audio_sinks(void) {
    if (audio_sinks) {
        for (int i = 0; i < audio_sink_count; i++)
            free(audio_sinks[i]);

        free(audio_sinks);
        audio_sinks = NULL;
        audio_sink_count = 0;
    }

    audio_sinks = str_parse_file(AUDIO_SINK_LIST, &audio_sink_count, parse_lines);
    if (audio_sink_count <= 0) return;

    add_drop_down_options(ui_dro_audio_sink_tweakgen, audio_sinks, audio_sink_count);

    const int live_sink = cfg_read_int(CONF_CONFIG_PATH "settings/general/audiosink", 0);
    lv_dropdown_set_selected(ui_dro_audio_sink_tweakgen, clamp_range(live_sink, 0, audio_sink_count - 1));

    lv_obj_clear_flag(ui_pnl_audio_sink_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static void tweakgen_refresh_task(lv_timer_t *timer) {
    ui_gen_refresh_task(timer);

    if (dialogue_active(&save_dlg)) return;
    if (++audio_sink_refresh_ticks < 30) return;
    audio_sink_refresh_ticks = 0;

    struct stat st;
    const long stamp = stat(AUDIO_SINK_LIST, &st) == 0 ? st.st_mtime : 0;
    int rebuilt = 0;

    if (stamp != audio_sink_stamp) {
        audio_sink_stamp = stamp;
        reload_audio_sinks();
        rebuilt = 1;
    }

    if (!rebuilt && lv_dropdown_get_selected(ui_dro_audio_sink_tweakgen) != audio_sink_original) return;

    const int live_sink = cfg_read_int(CONF_CONFIG_PATH "settings/general/audiosink", -1);
    if (live_sink < 0 || audio_sink_count <= 0) return;

    if (live_sink >= audio_sink_count) {
        const char *sink_args[] = {OPT_PATH "script/mux/audio_sink.sh", "list", NULL};
        run_exec(sink_args, A_SIZE(sink_args), 0, 1, NULL, NULL);

        audio_sink_stamp = stat(AUDIO_SINK_LIST, &st) == 0 ? st.st_mtime : 0;
        reload_audio_sinks();
        rebuilt = 1;

        if (live_sink >= audio_sink_count) {
            audio_sink_original = lv_dropdown_get_selected(ui_dro_audio_sink_tweakgen);
            return;
        }
    }

    char *live_name = audio_sink_name(live_sink);
    const int settled = live_name && strcmp(live_name, audio_sink_name_seen) == 0;
    const int sink_changed = settled && strcmp(live_name, audio_sink_name_last) != 0;

    snprintf(audio_sink_name_seen, sizeof(audio_sink_name_seen), "%s", live_name ? live_name : "");

    if (rebuilt || sink_changed || live_sink != audio_sink_original) {
        LOG_INFO(
            mux_module, "Sink list %s, live %d '%s', last known '%s', settled %d, %d listed",
            rebuilt ? "rebuilt" : "steady", live_sink, live_name ? live_name : "", audio_sink_name_last, settled,
            audio_sink_count
        );
    }

    if (!rebuilt && !sink_changed && live_sink == audio_sink_original) {
        free(live_name);
        return;
    }

    lv_dropdown_set_selected(ui_dro_audio_sink_tweakgen, live_sink);
    apply_option_value_long_dot(ui_dro_audio_sink_tweakgen);

    if (lv_group_get_focused(ui_group) == ui_lbl_audio_sink_tweakgen) {
        set_option_value_scroll_mode(ui_dro_audio_sink_tweakgen);
    }

    if (sink_changed) restore_sink_volume(live_sink, 0);

    free(live_name);

    audio_sink_original = live_sink;
}

static int visible_rgb(void) {
    return !lv_obj_has_flag(ui_pnl_rgb_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static int visible_distemp(void) {
    return !lv_obj_has_flag(ui_pnl_display_temp_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static int visible_brightness(void) {
    return !lv_obj_has_flag(ui_pnl_brightness_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static int visible_volume(void) {
    return !lv_obj_has_flag(ui_pnl_volume_tweakgen, LV_OBJ_FLAG_HIDDEN);
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

    if (audio_sink_count > 0) {
        audio_sink_volume_seed(audio_sink_active_index(), current_volume);

        const int active_sink =
            cfg_read_int(CONF_CONFIG_PATH "settings/general/audiosink", config.settings.general.audiosink);
        lv_dropdown_set_selected(ui_dro_audio_sink_tweakgen, clamp_range(active_sink, 0, audio_sink_count - 1));

        char *active_name = audio_sink_name(active_sink);
        snprintf(audio_sink_name_last, sizeof(audio_sink_name_last), "%s", active_name ? active_name : "");
        snprintf(audio_sink_name_seen, sizeof(audio_sink_name_seen), "%s", audio_sink_name_last);
        free(active_name);
    }

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
    int save_failed = 0;

    const char *startup_options[] = {"launcher", "explore", "collection", "history", "last", "resume"};

    CHECK_AND_SAVE_VAL(tweakgen, startup, "settings/general/startup", CHAR, startup_options);
    CHECK_AND_SAVE_STD(tweakgen, hk_dpad, "settings/hotkey/dpad_toggle", INT, 0);
    CHECK_AND_SAVE_STD(tweakgen, hk_shot, "settings/hotkey/screenshot", INT, 0);

    if (audio_sink_count > 0) {
        const int sink_mod = lv_dropdown_get_selected(ui_dro_audio_sink_tweakgen);

        if (sink_mod != audio_sink_original) {
            is_modified++;
            if (!write_text_to_file(CONF_CONFIG_PATH "settings/general/audiosink", "w", INT, sink_mod)) save_failed++;

            char idx_str[8];
            snprintf(idx_str, sizeof(idx_str), "%d", sink_mod);

            restore_sink_volume(sink_mod, 1);

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

    REPORT_SAVE_FAILURE();
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

    INIT_OPTION_ITEM(-1, tweakgen, brightness, lang.muxtweakgen.brightness, "brightness", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, volume, lang.muxtweakgen.volume, "volume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, audio_sink, lang.muxtweakgen.audiosink, "audiosink", audio_sinks, audio_sink_count);
    INIT_OPTION_ITEM(-1, tweakgen, hk_dpad, lang.muxtweakgen.hkdpad, "hkdpad", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, hk_shot, lang.muxtweakgen.hkshot, "hkshot", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, startup, lang.muxtweakgen.startup.title, "startup", startup_options, 6);
    INIT_OPTION_ITEM(-1, tweakgen, rtc, lang.muxtweakgen.rtc, "clock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, language, lang.muxtweakgen.language, "language", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, soundfont, lang.muxtweakgen.soundfont, "soundfont", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, hdmi, lang.muxtweakgen.hdmi, "hdmi", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, rgb, lang.muxtweakgen.rgb, "rgb", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, input_remap, lang.muxtweakgen.inputremap, "inputremap", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, pass_code, lang.muxtweakgen.passcode, "lock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, display_temp, lang.muxtweakgen.displaytemp, "displaytemp", NULL, 0);

    char *bright_pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_brightness_tweakgen, ui_dro_brightness_tweakgen, bright_pct_values);
    free(bright_pct_values);

    char *volume_pct_values = generate_number_string(0, audio_overdrive, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_volume_tweakgen, ui_dro_volume_tweakgen, volume_pct_values);
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

    static const list_frame frames[] = {
        {lang.muxtweakgen.section.options, tweakgen_off_options, tweakgen_len_options},
        {lang.muxtweakgen.section.submenu, tweakgen_off_submenu, tweakgen_len_submenu},
    };

    list_frame_init(
        &theme, ui_pnl_content, frames, A_SIZE(frames), ui_objects_panel, ui_objects, ui_objects_glyph,
        ui_objects_value, ui_count_dynamic
    );
    list_frame_apply();

    list_nav_move(list_frame_restore(), +1);
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    const int is_module = e_focused == ui_lbl_hdmi_tweakgen || e_focused == ui_lbl_rtc_tweakgen
                          || e_focused == ui_lbl_language_tweakgen || e_focused == ui_lbl_rgb_tweakgen
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
    gen_step_movement(steps, direction, 2, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

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

    if (e_focused == ui_lbl_volume_tweakgen) {
        const int v = lv_dropdown_get_selected(ui_dro_volume_tweakgen);

        if (v != volume_original) {
            toast_message(lang.muxtweakgen.volume_set, tst_wait_s);
            set_setting_value("audio", v, 0);
            volume_original = v;
            current_volume = v;
            audio_sink_volume_store(audio_sink_active_index(), v);
        }
    }
}

static void handle_frame_prev(void) {
    if (msgbox_active || dialogue_active(&save_dlg)) return;

    if (list_frame_move(-1)) {
        play_sound(snd_option);
        check_focus();
    }
}

static void handle_frame_next(void) {
    if (msgbox_active || dialogue_active(&save_dlg)) return;

    if (list_frame_move(+1)) {
        play_sound(snd_option);
        check_focus();
    }
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    if (dialogue_active(&save_dlg)) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (list_frame_focused()) {
        if (list_frame_move(-1)) {
            play_sound(snd_option);
            check_focus();
        }

        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    if (dialogue_active(&save_dlg)) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (list_frame_focused()) {
        if (list_frame_move(+1)) {
            play_sound(snd_option);
            check_focus();
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
    if (msgbox_active || block_input || dialogue_active(&save_dlg)) return;

    move_option(lv_group_get_focused(ui_group_value), -get_multi_count());
}

static void handle_option_next_multi(void) {
    if (msgbox_active || block_input || dialogue_active(&save_dlg)) return;

    move_option(lv_group_get_focused(ui_group_value), +get_multi_count());
}

typedef enum {
    menu_toggle = 0,
    menu_option,
    menu_clock,
    menu_hdmi,
    menu_rgb,
    menu_remap,
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

static int16_t kiosk_pass = 0;

static const menu_entry tweakgen_menu_entries[ui_count_dynamic] = {
    {NULL, &kiosk_pass, menu_option, visible_brightness}, // Brightness
    {NULL, &kiosk_pass, menu_option, visible_volume},     // Volume
    {NULL, &kiosk_pass, menu_toggle, visible_audiosink},
    {NULL, &kiosk_pass, menu_toggle, NULL}, // Hotkey DPAD
    {NULL, &kiosk_pass, menu_toggle, NULL}, // Hotkey Screenshot
    {NULL, &kiosk_pass, menu_toggle, NULL}, // Startup Mode
    {"rtc", &kiosk.datetime.clock, menu_clock, NULL},
    {"language", &kiosk.config.language, menu_clock, NULL},
    {"soundfont", &kiosk_pass, menu_clock, NULL},
    {"hdmi", &kiosk.setting.hdmi, menu_hdmi, visible_hdmi},
    {"rgb", &kiosk.setting.rgb, menu_rgb, visible_rgb},
    {"remap", &kiosk_pass, menu_remap, NULL},
    {"passcfg", &kiosk_pass, menu_passcode, NULL},
    {"distemp", &kiosk_pass, menu_display, visible_distemp}, // Colour Temperature
};

static void handle_save_mode(void) {
    const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
    char submenu[64];
    snprintf(submenu, sizeof(submenu), "%s", pending_submenu);
    hide_save_dialog();

    if (opt == mux_unsaved_save) save_tweak_options();

    if (submenu[0]) {
        list_frame_remember(lv_group_get_focused(ui_group));
        load_mux(submenu);
    } else {
        list_frame_remember_section();
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
    }

    mux_input_stop();
}

static void handle_menu_dispatch(void) {
    const int row = list_frame_current_row();
    if (row < 0 || row >= (int) A_SIZE(tweakgen_menu_entries)) return;

    const menu_entry *entry = &tweakgen_menu_entries[row];
    if (entry->visible && !entry->visible()) return;

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
                dialogue_open(&save_dlg, &theme);
                return;
            }

            play_sound(snd_confirm);
            save_tweak_options();

            list_frame_remember(lv_group_get_focused(ui_group));
            load_mux(entry->mux_name);

            mux_input_stop();
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

static void handle_a(void) {
    if (msgbox_active || block_input || hold_call) return;

    if (dialogue_active(&save_dlg)) {
        handle_save_mode();
        return;
    }

    handle_menu_dispatch();
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_tweakgen_modified()) {
        dialogue_open(&save_dlg, &theme);
        return;
    }

    play_sound(snd_back);
    save_tweak_options();

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (dialogue_active(&save_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&save_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad_hold(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad_hold(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || block_input || hold_call
        || dialogue_active(&save_dlg))
        return;

    play_sound(snd_info_open);
    show_help();
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
        lang.generic.select, lang.generic.cancel
    );
    init_timer(tweakgen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_frame_prev,
                [mux_input_l2] = handle_option_prev_multi,
                [mux_input_r1] = handle_frame_next,
                [mux_input_r2] = handle_option_next_multi,
            },
        .release_handler =
            {
                [mux_input_l2] = hold_call_release,
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_frame_prev,
            [mux_input_l2] = hold_call_set,
            [mux_input_r1] = handle_frame_next,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxtweakgen.title, lang.muxtweakgen.overview);

    mux_input_task(&input_opts);

    return 0;
}
