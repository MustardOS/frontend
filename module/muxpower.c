#include "muxshare.h"
#include "ui/ui_muxpower.h"

#define UI_COUNT 4

#define POWER(NAME, UDATA) static int NAME##_original;
    POWER_ELEMENTS
#undef POWER

#define SHUTDOWN_COUNT 12
static const int shutdown_values[] = {-2, -1, 2, 10, 30, 60, 120, 300, 600, 900, 1800, 3600};

#define BATTERY_COUNT 11
static const int battery_values[] = {-255, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};

#define IDLE_COUNT 9
static const int idle_values[] = {0, 10, 30, 60, 120, 300, 600, 900, 1800};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblShutdown_power,    lang.MUXPOWER.HELP.SLEEP_FUNCTION},
            {ui_lblBattery_power,     lang.MUXPOWER.HELP.LOW_BATTERY},
            {ui_lblIdleDisplay_power, lang.MUXPOWER.HELP.IDLE_DISPLAY},
            {ui_lblIdleSleep_power,   lang.MUXPOWER.HELP.IDLE_SLEEP},
    };

    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);
    gen_help(element_focused, help_messages, num_messages);
}

static void init_dropdown_settings() {
#define POWER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_power);
    POWER_ELEMENTS
#undef POWER
}

static void restore_tweak_options() {
    lv_obj_t *ui_droIdle[2] = {ui_droIdleDisplay_power, ui_droIdleSleep_power};
    int16_t *config_values[2] = {&config.SETTINGS.POWER.IDLE_DISPLAY, &config.SETTINGS.POWER.IDLE_SLEEP};

    map_drop_down_to_index(ui_droShutdown_power, config.SETTINGS.POWER.SHUTDOWN, shutdown_values, SHUTDOWN_COUNT, 0);
    map_drop_down_to_index(ui_droBattery_power, config.SETTINGS.POWER.LOW_BATTERY, battery_values, BATTERY_COUNT, 5);

    for (int i = 0; i < 2; i++) {
        int is_custom = 1;
        for (size_t j = 0; j < sizeof(idle_values) / sizeof(idle_values[0]); j++) {
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

static int save_tweak_options() {
    int is_modified = 0;

    int idx_shutdown = map_drop_down_to_value(lv_dropdown_get_selected(ui_droShutdown_power),
                                              shutdown_values, SHUTDOWN_COUNT, -2);
    int idx_battery = map_drop_down_to_value(lv_dropdown_get_selected(ui_droBattery_power),
                                             battery_values, BATTERY_COUNT, 25);
    int idx_idle_display = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleDisplay_power),
                                                  idle_values, IDLE_COUNT, 0);
    int idx_idle_sleep = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleSleep_power),
                                                idle_values, IDLE_COUNT, 0);

    if (idx_idle_display && idx_idle_sleep && idx_idle_display >= idx_idle_sleep) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXPOWER.IDLE_ERROR, 2000);
        refresh_screen(ui_screen);
        return 0;
    }

    play_sound(SND_BACK);

    if (lv_dropdown_get_selected(ui_droShutdown_power) != Shutdown_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/shutdown"), "w", INT, idx_shutdown);
    }

    if (lv_dropdown_get_selected(ui_droBattery_power) != Battery_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/low_battery"), "w", INT, idx_battery);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleDisplay_power) > 1 &&
        lv_dropdown_get_selected(ui_droIdleDisplay_power) != IdleDisplay_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/idle_display"), "w", INT, idx_idle_display);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleSleep_power) > 1 &&
        lv_dropdown_get_selected(ui_droIdleSleep_power) != IdleSleep_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/idle_sleep"), "w", INT, idx_idle_sleep);
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);

        const char *args[] = {(INTERNAL_PATH "script/mux/tweak.sh"), NULL};
        run_exec(args, A_SIZE(args), 0);

        refresh_config = 1;
    }

    return 1;
}

static void init_navigation_group() {
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
    INIT_OPTION_ITEM(-1, power, IdleDisplay, lang.MUXPOWER.IDLE.DISPLAY, "idle_display", idle_timer, IDLE_COUNT);
    INIT_OPTION_ITEM(-1, power, IdleSleep, lang.MUXPOWER.IDLE.SLEEP, "idle_sleep", idle_timer, IDLE_COUNT);

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

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (save_tweak_options()) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");

        close_input();
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
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

int muxpower_main() {
    init_module("muxpower");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPOWER.TITLE);
    init_muxpower(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
