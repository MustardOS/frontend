#include "muxshare.h"
#include "ui/ui_muxpower.h"

#define UI_COUNT 7

#define POWER(NAME, UDATA) static int NAME##_original;
POWER_ELEMENTS
#undef POWER

#define SHUTDOWN_COUNT 12
static const int shutdown_values[] = {-2, -1, 2, 10, 30, 60, 120, 300, 600, 900, 1800, 3600};

#define BATTERY_COUNT 11
static const int battery_values[] = {-255, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};

#define IDLE_COUNT 9
static const int idle_values[] = {0, 10, 30, 60, 120, 300, 600, 900, 1800};

char **gov_values_lower = NULL;
char **gov_values_disp = NULL;

size_t gov_count = 0;

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblShutdown_power,    lang.MUXPOWER.HELP.SLEEP_FUNCTION},
            {ui_lblBattery_power,     lang.MUXPOWER.HELP.LOW_BATTERY},
            {ui_lblIdleDisplay_power, lang.MUXPOWER.HELP.IDLE.DISPLAY},
            {ui_lblIdleSleep_power,   lang.MUXPOWER.HELP.IDLE.SLEEP},
            {ui_lblIdleMute_power,    lang.MUXPOWER.HELP.IDLE.MUTE},
            {ui_lblGovIdle_power,     lang.MUXPOWER.HELP.GOV.IDLE},
            {ui_lblGovDefault_power,  lang.MUXPOWER.HELP.GOV.DEFAULT},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define POWER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_power);
    POWER_ELEMENTS
#undef POWER
}

static void generate_governor_values(void) {
    int count = 0;
    char **list = str_parse_file(device.CPU.AVAILABLE, &count, TOKENS);

    if (gov_values_lower) {
        for (size_t i = 0; i < gov_count; i++) free(gov_values_lower[i]);
        free(gov_values_lower);
        gov_values_lower = NULL;
    }

    if (gov_values_disp) {
        for (size_t i = 0; i < gov_count; i++) free(gov_values_disp[i]);
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

    for (int i = 0; i < count; i++) free(list[i]);
    free(list);
}

static void free_governor_values(void) {
    if (gov_values_lower) {
        for (size_t i = 0; i < gov_count; i++) free(gov_values_lower[i]);
        free(gov_values_lower);
        gov_values_lower = NULL;
    }

    if (gov_values_disp) {
        for (size_t i = 0; i < gov_count; i++) free(gov_values_disp[i]);
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

static void restore_power_options(void) {
    lv_obj_t *ui_droIdle[2] = {ui_droIdleDisplay_power, ui_droIdleSleep_power};
    int16_t *config_values[2] = {&config.SETTINGS.POWER.IDLE.DISPLAY, &config.SETTINGS.POWER.IDLE.SLEEP};

    map_drop_down_to_index(ui_droShutdown_power, config.SETTINGS.POWER.SHUTDOWN, shutdown_values, SHUTDOWN_COUNT, 0);
    map_drop_down_to_index(ui_droBattery_power, config.SETTINGS.POWER.LOW_BATTERY, battery_values, BATTERY_COUNT, 5);

    lv_dropdown_set_selected(ui_droIdleMute_power, config.SETTINGS.POWER.IDLE.MUTE);

    lv_dropdown_set_selected(ui_droGovIdle_power, find_governor(config.SETTINGS.POWER.GOV.IDLE));
    lv_dropdown_set_selected(ui_droGovDefault_power, find_governor(config.SETTINGS.POWER.GOV.DEFAULT));

    for (int i = 0; i < 2; i++) {
        int is_custom = 1;
        for (size_t j = 0; j < A_SIZE(idle_values); j++) {
            if (*config_values[i] == idle_values[j]) {
                is_custom = 0;
                break;
            }
        }

        if (is_custom) {
            lv_dropdown_clear_options(ui_droIdle[i]);
            lv_dropdown_add_option(ui_droIdle[i], lang.GENERIC.USER_DEFINED, 0);
        } else {
            map_drop_down_to_index(ui_droIdle[i], *config_values[i], idle_values, IDLE_COUNT + 1, 0);
        }
    }
}

static int save_power_options(void) {
    int idx_idle_display = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleDisplay_power),
                                                  idle_values, IDLE_COUNT, 0);
    int idx_idle_sleep = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleSleep_power),
                                                idle_values, IDLE_COUNT, 0);

    if (idx_idle_display && idx_idle_sleep && idx_idle_display >= idx_idle_sleep) {
        play_sound(SND_ERROR);

        toast_message(lang.MUXPOWER.IDLE.ERROR, LONG);
        refresh_screen(ui_screen);

        return 0;
    }

    int is_modified = 0;

    int idx_shutdown = map_drop_down_to_value(lv_dropdown_get_selected(ui_droShutdown_power),
                                              shutdown_values, SHUTDOWN_COUNT, -2);
    int idx_battery = map_drop_down_to_value(lv_dropdown_get_selected(ui_droBattery_power),
                                             battery_values, BATTERY_COUNT, 25);

    if (lv_dropdown_get_selected(ui_droShutdown_power) != Shutdown_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/shutdown", "w", INT, idx_shutdown);
    }

    if (lv_dropdown_get_selected(ui_droBattery_power) != Battery_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/low_battery", "w", INT, idx_battery);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleDisplay_power) > 1 &&
        lv_dropdown_get_selected(ui_droIdleDisplay_power) != IdleDisplay_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/idle_display", "w", INT, idx_idle_display);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleSleep_power) > 1 &&
        lv_dropdown_get_selected(ui_droIdleSleep_power) != IdleSleep_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/power/idle_sleep", "w", INT, idx_idle_sleep);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleMute_power) > 1 &&
        lv_dropdown_get_selected(ui_droIdleMute_power) != IdleMute_original) {
        is_modified++;
        CHECK_AND_SAVE_STD(power, IdleMute, "settings/power/idle_mute", INT, 0);
    }

    if (lv_dropdown_get_option_cnt(ui_droGovIdle_power) > 1 &&
        lv_dropdown_get_selected(ui_droGovIdle_power) != GovIdle_original) {
        is_modified++;
        CHECK_AND_SAVE_VAL(power, GovIdle, "settings/power/gov_idle", CHAR, gov_values_lower);
    }

    if (lv_dropdown_get_option_cnt(ui_droGovDefault_power) > 1 &&
        lv_dropdown_get_selected(ui_droGovDefault_power) != GovDefault_original) {
        is_modified++;
        CHECK_AND_SAVE_DEV_VAL(power, GovDefault, "cpu/default", CHAR, gov_values_lower);
    }

    if (is_modified > 0) run_tweak_script();

    free_governor_values();
    play_sound(SND_BACK);

    return 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *sleep_timer[] = {
            lang.GENERIC.DISABLED, lang.MUXPOWER.SLEEP.SUSPEND, lang.MUXPOWER.SLEEP.INSTANT,
            lang.MUXPOWER.SLEEP.t10s, lang.MUXPOWER.SLEEP.t30s, lang.MUXPOWER.SLEEP.t60s,
            lang.MUXPOWER.SLEEP.t2m, lang.MUXPOWER.SLEEP.t5m, lang.MUXPOWER.SLEEP.t10m,
            lang.MUXPOWER.SLEEP.t15m, lang.MUXPOWER.SLEEP.t30m, lang.MUXPOWER.SLEEP.t60m
    };

    char *idle_timer[] = {
            lang.GENERIC.DISABLED, lang.MUXPOWER.IDLE.t10s, lang.MUXPOWER.IDLE.t30s,
            lang.MUXPOWER.IDLE.t60s, lang.MUXPOWER.IDLE.t2m, lang.MUXPOWER.IDLE.t5m,
            lang.MUXPOWER.IDLE.t10m, lang.MUXPOWER.IDLE.t15m, lang.MUXPOWER.IDLE.t30m
    };

    INIT_OPTION_ITEM(-1, power, Shutdown, lang.MUXPOWER.SLEEP.TITLE, "shutdown", sleep_timer, SHUTDOWN_COUNT);
    INIT_OPTION_ITEM(-1, power, Battery, lang.MUXPOWER.LOW_BATTERY, "battery", NULL, 0);
    INIT_OPTION_ITEM(-1, power, IdleSleep, lang.MUXPOWER.IDLE.SLEEP, "idle_sleep", idle_timer, IDLE_COUNT);
    INIT_OPTION_ITEM(-1, power, IdleDisplay, lang.MUXPOWER.IDLE.DISPLAY, "idle_display", idle_timer, IDLE_COUNT);
    INIT_OPTION_ITEM(-1, power, IdleMute, lang.MUXPOWER.IDLE.MUTE, "idle_mute", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, power, GovIdle, lang.MUXPOWER.GOV.IDLE, "gov_idle", gov_values_disp, (int) gov_count);
    INIT_OPTION_ITEM(-1, power, GovDefault, lang.MUXPOWER.GOV.DEFAULT, "gov_fe", gov_values_disp, (int) gov_count);

    char *battery_string = generate_number_string(5, 50, 5, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBattery_power, battery_string);
    free(battery_string);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
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

    if (save_power_options()) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");

        close_input();
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define POWER(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_power, UDATA);
    POWER_ELEMENTS
#undef POWER

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxpower_main(void) {
    init_module("muxpower");

    init_theme(1, 0);
    generate_governor_values();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPOWER.TITLE);
    init_muxpower(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_power_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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

    return 0;
}
