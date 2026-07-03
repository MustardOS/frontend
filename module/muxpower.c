#include "muxshare.h"
#include "ui/ui_muxpower.h"

static int save_mode = 0;
static mux_dialogue save_dlg;

static int wr_dlg_active = 0;
static mux_dialogue wr_dlg;

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
}

#define POWER(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(POWER_ELEMENTS) };
#undef POWER

#define POWER(NAME, UDATA) static int NAME##_original;
POWER_ELEMENTS
#undef POWER

static int any_power_modified(void) {
#define POWER(NAME, UDATA)                                                                                             \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_power) != NAME##_original) return 1;
    POWER_ELEMENTS
#undef POWER
    return 0;
}

#define SHUTDOWN_COUNT 12
static const int shutdown_values[] = {-2, -1, 2, 10, 30, 60, 120, 300, 600, 900, 1800, 3600};

#define IDLE_COUNT 9
static const int idle_values[] = {0, 10, 30, 60, 120, 300, 600, 900, 1800};

#define SAVER_SPEED_COUNT 4
enum saver_speed {
    saver_speed_crawl = 30,
    saver_speed_cruise = 90,
    saver_speed_fast = 150,
    saver_speed_turbo = 300,
    saver_speed_ludicrous = 600
};

static const int saver_speed_values[SAVER_SPEED_COUNT] = {
    saver_speed_crawl, saver_speed_cruise, saver_speed_fast, saver_speed_turbo
};

#define SAVER_TYPE_COUNT 16
enum saver_type {
    saver_type_disabled = 0,
    saver_type_dvd = 1,
    saver_type_star = 2,
    saver_type_matrix = 3,
    saver_type_firefly = 4,
    saver_type_pulse = 5,
    saver_type_trace = 6,
    saver_type_constellation = 7,
    saver_type_mystify = 8,
    saver_type_maze = 9,
    saver_type_blockfall = 10,
    saver_type_datetime = 11,
    saver_type_video = 12,
    saver_type_slideshow = 13,
    saver_type_boxart = 14,
    saver_type_bsod = 15,
};

static const int saver_type_values[SAVER_TYPE_COUNT] = {
    saver_type_disabled,  saver_type_dvd,      saver_type_star,          saver_type_matrix,    saver_type_firefly,
    saver_type_pulse,     saver_type_trace,    saver_type_constellation, saver_type_mystify,   saver_type_maze,
    saver_type_blockfall, saver_type_datetime, saver_type_video,         saver_type_slideshow, saver_type_boxart,
    saver_type_bsod,
};

char **gov_values_lower = NULL;
char **gov_values_disp = NULL;

size_t gov_count = 0;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define POWER(NAME, UDATA) {UDATA, lang.muxpower.help.NAME},
        POWER_ELEMENTS
#undef POWER
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define POWER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_power);
    POWER_ELEMENTS
#undef POWER

    battery_original = pct_to_int(lv_dropdown_get_selected(ui_dro_battery_power), 0, 100);
}

static void generate_governor_values(void) {
    int count = 0;
    char **list = str_parse_file(device.cpu.available, &count, parse_tokens);

    if (gov_values_lower) {
        for (size_t i = 0; i < gov_count; i++)
            free(gov_values_lower[i]);
        free(gov_values_lower);
        gov_values_lower = NULL;
    }

    if (gov_values_disp) {
        for (size_t i = 0; i < gov_count; i++)
            free(gov_values_disp[i]);
        free(gov_values_disp);
        gov_values_disp = NULL;
    }

    gov_count = 0;

    if (!list || count <= 0) {
        static const char *fallback[] = {"performance", "powersave"};
        gov_count = sizeof(fallback) / sizeof(fallback[0]);

        gov_values_lower = calloc(gov_count, sizeof(char *));
        gov_values_disp = calloc(gov_count, sizeof(char *));
        for (size_t i = 0; i < gov_count; i++) {
            gov_values_lower[i] = strdup(fallback[i]);
            gov_values_disp[i] = strdup(fallback[i]);
            str_capital(gov_values_disp[i]);
        }

        if (list) free(list);
        return;
    }

    gov_count = (size_t) count;
    gov_values_lower = calloc(gov_count, sizeof(char *));
    gov_values_disp = calloc(gov_count, sizeof(char *));
    for (size_t i = 0; i < gov_count; i++) {
        gov_values_lower[i] = strdup(list[i]);
        gov_values_disp[i] = strdup(list[i]);
        str_capital(gov_values_disp[i]);
    }

    for (int i = 0; i < count; i++)
        free(list[i]);
    free(list);
}

static void free_governor_values(void) {
    if (gov_values_lower) {
        for (size_t i = 0; i < gov_count; i++)
            free(gov_values_lower[i]);
        free(gov_values_lower);
        gov_values_lower = NULL;
    }

    if (gov_values_disp) {
        for (size_t i = 0; i < gov_count; i++)
            free(gov_values_disp[i]);
        free(gov_values_disp);
        gov_values_disp = NULL;
    }

    gov_count = 0;
}

static int find_governor(char *governor) {
    if (!gov_values_lower || !governor) return 0;

    for (size_t i = 0; i < gov_count; i++) {
        if (gov_values_lower[i] && strcmp(gov_values_lower[i], governor) == 0) return (int) i;
    }

    return 0;
}

static void set_saver(void) {
    const uint16_t cnt = lv_dropdown_get_option_cnt(ui_dro_saver_speed_power);
    if (cnt <= SAVER_SPEED_COUNT)
        lv_dropdown_add_option(ui_dro_saver_speed_power, lang.generic.ludicrous, LV_DROPDOWN_POS_LAST);

    lv_dropdown_set_selected(ui_dro_saver_speed_power, SAVER_SPEED_COUNT);
    play_sound(snd_muos);

    toast_message("\x54\x68\x65\x79\x27\x76\x65\x20\x67\x6F\x6E\x65\x20\x70\x6C\x61\x69\x64\x2E\x2E\x2E", tst_wait_s);

    refresh_screen(ui_screen, 1);
}

static int get_selected_saver_type(void) {
    return map_drop_down_to_value(
        lv_dropdown_get_selected(ui_dro_saver_type_power), saver_type_values, SAVER_TYPE_COUNT, saver_type_dvd
    );
}

static int get_selected_saver_speed(void) {
    const uint16_t selected = lv_dropdown_get_selected(ui_dro_saver_speed_power);

    if (selected == SAVER_SPEED_COUNT) return saver_speed_ludicrous;
    return map_drop_down_to_value(selected, saver_speed_values, SAVER_SPEED_COUNT, saver_speed_crawl);
}

static int is_saver_preview_item(const lv_obj_t *e_focused) {
    return e_focused == ui_dro_saver_type_power || e_focused == ui_dro_saver_speed_power;
}

static int saver_type_enabled(void) {
    return get_selected_saver_type() != saver_type_disabled;
}

static int saver_preview_focus_active(void) {
    const lv_obj_t *focused = lv_group_get_focused(ui_group);

    return saver_type_enabled() && (focused == ui_lbl_saver_type_power || focused == ui_lbl_saver_speed_power);
}

static void check_focus(void) {
    if (saver_type_enabled()) {
        lv_obj_clear_flag(ui_pnl_saver_speed_power, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_pnl_saver_speed_power, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (saver_preview_focus_active()) {
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void restore_power_options(void) {
    lv_obj_t *ui_dro_idle[2] = {ui_dro_idle_display_power, ui_dro_idle_sleep_power};
    int16_t *config_values[2] = {&config.settings.power.idle.display, &config.settings.power.idle.sleep};

    map_drop_down_to_index(ui_dro_shutdown_power, config.settings.power.shutdown, shutdown_values, SHUTDOWN_COUNT, 0);

    lv_dropdown_set_selected(ui_dro_battery_power, int_to_pct(config.settings.power.low_battery, 0, 100));
    lv_dropdown_set_selected(ui_dro_idle_mute_power, config.settings.power.idle.mute);
    lv_dropdown_set_selected(ui_dro_gov_idle_power, find_governor(config.settings.power.gov.idle));
    lv_dropdown_set_selected(ui_dro_gov_default_power, find_governor(config.settings.power.gov.dflt));

    if (config.settings.power.saver_speed == saver_speed_ludicrous) {
        const uint16_t cnt = lv_dropdown_get_option_cnt(ui_dro_saver_speed_power); // heh...
        if (cnt <= SAVER_SPEED_COUNT)
            lv_dropdown_add_option(ui_dro_saver_speed_power, lang.generic.ludicrous, LV_DROPDOWN_POS_LAST);
        lv_dropdown_set_selected(ui_dro_saver_speed_power, SAVER_SPEED_COUNT);
    } else {
        map_drop_down_to_index(
            ui_dro_saver_speed_power, config.settings.power.saver_speed, saver_speed_values, SAVER_SPEED_COUNT, 0
        );
    }

    map_drop_down_to_index(
        ui_dro_saver_type_power, config.settings.power.saver_type, saver_type_values, SAVER_TYPE_COUNT, saver_type_dvd
    );

    for (int i = 0; i < 2; i++) {
        int is_custom = 1;
        for (size_t j = 0; j < A_SIZE(idle_values); j++) {
            if (*config_values[i] == idle_values[j]) {
                is_custom = 0;
                break;
            }
        }

        if (is_custom) {
            lv_dropdown_clear_options(ui_dro_idle[i]);
            lv_dropdown_add_option(ui_dro_idle[i], lang.generic.user_defined, 0);
        } else {
            map_drop_down_to_index(ui_dro_idle[i], *config_values[i], idle_values, IDLE_COUNT + 1, 0);
        }
    }

    check_focus();
}

static int save_power_options(void) {
    const int idx_idle_display =
        map_drop_down_to_value(lv_dropdown_get_selected(ui_dro_idle_display_power), idle_values, IDLE_COUNT, 0);
    const int idx_idle_sleep =
        map_drop_down_to_value(lv_dropdown_get_selected(ui_dro_idle_sleep_power), idle_values, IDLE_COUNT, 0);

    if (idx_idle_display && idx_idle_sleep && idx_idle_display >= idx_idle_sleep) {
        play_sound(snd_error);

        toast_message(lang.muxpower.idle.error, tst_wait_l);
        refresh_screen(ui_screen, 1);

        return 0;
    }

    int is_modified = 0;

    const int idx_shutdown =
        map_drop_down_to_value(lv_dropdown_get_selected(ui_dro_shutdown_power), shutdown_values, SHUTDOWN_COUNT, -2);

    if (lv_dropdown_get_selected(ui_dro_shutdown_power) != shutdown_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/shutdown", "w", INT, idx_shutdown);
    }

    const int idx_battery = pct_to_int(lv_dropdown_get_selected(ui_dro_battery_power), 0, 100);
    if (lv_dropdown_get_selected(ui_dro_battery_power) != battery_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/low_battery", "w", INT, idx_battery);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_idle_display_power) > 1
        && lv_dropdown_get_selected(ui_dro_idle_display_power) != idle_display_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/idle_display", "w", INT, idx_idle_display);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_idle_sleep_power) > 1
        && lv_dropdown_get_selected(ui_dro_idle_sleep_power) != idle_sleep_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/idle_sleep", "w", INT, idx_idle_sleep);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_idle_mute_power) > 1
        && lv_dropdown_get_selected(ui_dro_idle_mute_power) != idle_mute_original) {
        is_modified++;
        CHECK_AND_SAVE_STD(power, idle_mute, "settings/power/idle_mute", INT, 0);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_gov_idle_power) > 1
        && lv_dropdown_get_selected(ui_dro_gov_idle_power) != gov_idle_original) {
        is_modified++;
        CHECK_AND_SAVE_VAL(power, gov_idle, "settings/power/gov_idle", CHAR, gov_values_lower);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_gov_default_power) > 1
        && lv_dropdown_get_selected(ui_dro_gov_default_power) != gov_default_original) {
        is_modified++;
        CHECK_AND_SAVE_DEV_VAL(power, gov_default, "cpu/default", CHAR, gov_values_lower);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_saver_speed_power) > 1
        && lv_dropdown_get_selected(ui_dro_saver_speed_power) != saver_speed_original) {
        int ss_value;
        if (lv_dropdown_get_selected(ui_dro_saver_speed_power) == SAVER_SPEED_COUNT) {
            ss_value = saver_speed_ludicrous;
        } else {
            ss_value = map_drop_down_to_value(
                lv_dropdown_get_selected(ui_dro_saver_speed_power), saver_speed_values, SAVER_SPEED_COUNT,
                saver_speed_crawl
            );
        }

        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/saver_speed", "w", INT, ss_value);
    }

    if (lv_dropdown_get_option_cnt(ui_dro_saver_type_power) > 1
        && lv_dropdown_get_selected(ui_dro_saver_type_power) != saver_type_original) {
        const int idx_type = map_drop_down_to_value(
            lv_dropdown_get_selected(ui_dro_saver_type_power), saver_type_values, SAVER_TYPE_COUNT, saver_type_dvd
        );
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/saver_type", "w", INT, idx_type);
    }

    if (is_modified > 0) run_tweak_script(lang.generic.saving);

    if (get_selected_saver_type() == saver_type_disabled && idx_idle_display == 300 && idx_idle_sleep == 0
        && !file_exist(WR_PATH)) {
        write_text_to_file(
            WR_PATH, "w", CHAR, MUX_LOG_TAG_HDR FIO_REC_HDR_FMT SND_CHUNK_HDR OVR_PACK_FMT DT_ZONE_FMT CLR_MAP_HDR
        );
        dialogue_init_accept(
            &wr_dlg, &theme, ui_screen, "\x4b\x6e\x6f\x63\x6b\x2c\x20\x6b\x6e\x6f\x63\x6b\x2c\x20\x4e\x65\x6f\x2e",
            "\x44\x6f\x20\x79\x6f\x75\x20\x61\x63\x63\x65\x70\x74\x20\x74\x68\x65\x20\x63\x61\x6c\x6c\x3f",
            lang.generic.confirm
        );
        dialogue_show(&wr_dlg);

        wr_dlg_active = 1;
        msgbox_active = 1;

        free_governor_values();
        return 0;
    }

    free_governor_values();
    play_sound(snd_back);

    return 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *sleep_timer[] = {lang.generic.disabled,     lang.muxpower.sleep.suspend, lang.muxpower.sleep.instant,
                           lang.muxpower.sleep.t10_s, lang.muxpower.sleep.t30_s,   lang.muxpower.sleep.t60_s,
                           lang.muxpower.sleep.t2_m,  lang.muxpower.sleep.t5_m,    lang.muxpower.sleep.t10_m,
                           lang.muxpower.sleep.t15_m, lang.muxpower.sleep.t30_m,   lang.muxpower.sleep.t60_m};

    char *idle_timer[] = {lang.generic.disabled,    lang.muxpower.idle.t10_s, lang.muxpower.idle.t30_s,
                          lang.muxpower.idle.t60_s, lang.muxpower.idle.t2_m,  lang.muxpower.idle.t5_m,
                          lang.muxpower.idle.t10_m, lang.muxpower.idle.t15_m, lang.muxpower.idle.t30_m};

    static const char *saver_speed[SAVER_SPEED_COUNT] = {
        lang.muxpower.saver.speed.crawl, lang.muxpower.saver.speed.cruise, lang.muxpower.saver.speed.fast,
        lang.muxpower.saver.speed.turbo
    };

    static const char *saver_type[SAVER_TYPE_COUNT] = {
        lang.generic.disabled,           lang.muxpower.saver.type.dvd,           lang.muxpower.saver.type.star,
        lang.muxpower.saver.type.matrix, lang.muxpower.saver.type.firefly,       lang.muxpower.saver.type.pulse,
        lang.muxpower.saver.type.trace,  lang.muxpower.saver.type.constellation, lang.muxpower.saver.type.mystify,
        lang.muxpower.saver.type.maze,   lang.muxpower.saver.type.blockfall,     lang.muxpower.saver.type.datetime,
        lang.muxpower.saver.type.video,  lang.muxpower.saver.type.slideshow,     lang.muxpower.saver.type.boxart,
        lang.muxpower.saver.type.bsod,
    };

    INIT_OPTION_ITEM(-1, power, shutdown, lang.muxpower.sleep.title, "shutdown", sleep_timer, SHUTDOWN_COUNT);
    INIT_OPTION_ITEM(-1, power, battery, lang.muxpower.low_battery, "battery", NULL, 0);
    INIT_OPTION_ITEM(-1, power, idle_sleep, lang.muxpower.idle.sleep, "idle_sleep", idle_timer, IDLE_COUNT);
    INIT_OPTION_ITEM(-1, power, idle_display, lang.muxpower.idle.display, "idle_display", idle_timer, IDLE_COUNT);
    INIT_OPTION_ITEM(-1, power, idle_mute, lang.muxpower.idle.mute, "idle_mute", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, power, gov_idle, lang.muxpower.gov.idle, "gov_idle", gov_values_disp, (int) gov_count);
    INIT_OPTION_ITEM(-1, power, gov_default, lang.muxpower.gov.dflt, "gov_fe", gov_values_disp, (int) gov_count);
    INIT_OPTION_ITEM(
        -1, power, saver_type, lang.muxpower.saver.type.title, "saver_type", (char **) saver_type, SAVER_TYPE_COUNT
    );
    INIT_OPTION_ITEM(
        -1, power, saver_speed, lang.muxpower.saver.speed.title, "saver_speed", (char **) saver_speed, SAVER_SPEED_COUNT
    );

    char *battery_pct = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_dro_battery_power, battery_pct);
    free(battery_pct);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
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

static void handle_option_prev(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    lv_obj_t *e_focused = lv_group_get_focused(ui_group_value);
    move_option(e_focused, -1);

    if (e_focused == ui_dro_saver_type_power) check_focus();
}

static void handle_option_next(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    lv_obj_t *e_focused = lv_group_get_focused(ui_group_value);
    move_option(e_focused, +1);

    if (e_focused == ui_dro_saver_type_power) check_focus();
}

static void handle_a(void) {
    if (wr_dlg_active) {
        wr_dlg_active = 0;
        msgbox_active = 0;
        dialogue_hide(&wr_dlg);
        play_sound(snd_confirm);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");
        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) {
            if (!save_power_options()) {
                return;
            }
        }

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");
        mux_input_stop();

        return;
    }

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (wr_dlg_active) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_power_modified()) {
        show_save_dialog();
        return;
    }

    if (save_power_options()) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");

        mux_input_stop();
    }
}

static void handle_dpad_up(void) {
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
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_x(void) {
    if (msgbox_active || hold_call || save_mode) return;

    const lv_obj_t *e_focused = lv_group_get_focused(ui_group_value);
    if (!is_saver_preview_item(e_focused)) return;

    const int type = get_selected_saver_type();
    if (type == saver_type_disabled) return;

    play_sound(snd_confirm);
    preview_saver(type, get_selected_saver_speed());

    refresh_screen(ui_screen, 1);
}

static void handle_y(void) {
    if (msgbox_active || hold_call || save_mode) return;

    if (lv_group_get_focused(ui_group_value) == ui_dro_saver_speed_power) set_saver();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.preview, 0},
                                  {NULL, NULL, 0}});

    check_focus();

#define POWER(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_power, UDATA);
    POWER_ELEMENTS
#undef POWER

    overlay_display();
}

int muxpower_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    generate_governor_values();

    init_ui_common_screen(&theme, &device, &lang, lang.muxpower.title);
    init_muxpower(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_power_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
    );

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

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
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
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
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
