#include "muxshare.h"
#include "muxpower.h"
#include "ui/ui_muxpower.h"
#include <string.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int shutdown_original, battery_original, idle_display_original, idle_sleep_original;

#define UI_COUNT 4
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

static const int shutdown_values[] = {-2, -1, 2, 10, 30, 60, 120, 300, 600, 900, 1800, 3600};
static const int battery_values[] = {-255, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
static const int idle_values[] = {0, 10, 30, 60, 120, 300, 600, 900, 1800};
#define SHUTDOWN_COUNT 12
#define BATTERY_COUNT 11
#define IDLE_COUNT 9

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblShutdown,    lang.MUXPOWER.HELP.SLEEP_FUNCTION},
            {ui_lblBattery,     lang.MUXPOWER.HELP.LOW_BATTERY},
            {ui_lblIdleDisplay, lang.MUXPOWER.HELP.IDLE_DISPLAY},
            {ui_lblIdleSleep,   lang.MUXPOWER.HELP.IDLE_SLEEP},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void init_dropdown_settings() {
    shutdown_original = lv_dropdown_get_selected(ui_droShutdown);
    battery_original = lv_dropdown_get_selected(ui_droBattery_power);
    idle_display_original = lv_dropdown_get_selected(ui_droIdleDisplay);
    idle_sleep_original = lv_dropdown_get_selected(ui_droIdleSleep);
}

static void restore_tweak_options() {
    lv_obj_t *ui_droIdle[2] = {ui_droIdleDisplay, ui_droIdleSleep};
    int16_t *config_values[2] = {&config.SETTINGS.POWER.IDLE_DISPLAY, &config.SETTINGS.POWER.IDLE_SLEEP};

    map_drop_down_to_index(ui_droShutdown, config.SETTINGS.POWER.SHUTDOWN, shutdown_values, SHUTDOWN_COUNT, 0);
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
            map_drop_down_to_index(ui_droIdle[i], *config_values[i], idle_values, 10, 0);
        }
    }
}

static void save_tweak_options() {
    int idx_shutdown = map_drop_down_to_value(lv_dropdown_get_selected(ui_droShutdown),
                                              shutdown_values, SHUTDOWN_COUNT, -2);

    int idx_battery = map_drop_down_to_value(lv_dropdown_get_selected(ui_droBattery_power),
                                             battery_values, BATTERY_COUNT, 25);

    int idx_idle_display = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleDisplay),
                                                  idle_values, IDLE_COUNT, 0);

    int idx_idle_sleep = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleSleep),
                                                idle_values, IDLE_COUNT, 0);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droShutdown) != shutdown_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/shutdown"), "w", INT, idx_shutdown);
    }

    if (lv_dropdown_get_selected(ui_droBattery_power) != battery_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/low_battery"), "w", INT, idx_battery);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleDisplay) > 1 &&
        lv_dropdown_get_selected(ui_droIdleDisplay) != idle_display_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/power/idle_display"), "w", INT, idx_idle_display);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleSleep) > 1 &&
        lv_dropdown_get_selected(ui_droIdleSleep) != idle_sleep_original) {
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
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBattery,
            ui_pnlIdleDisplay,
            ui_pnlIdleSleep,
            ui_pnlShutdown
    };

    ui_objects[0] = ui_lblBattery;
    ui_objects[1] = ui_lblIdleDisplay;
    ui_objects[2] = ui_lblIdleSleep;
    ui_objects[3] = ui_lblShutdown;
    lv_obj_t *ui_objects_value[] = {
            ui_droBattery_power,
            ui_droIdleDisplay,
            ui_droIdleSleep,
            ui_droShutdown
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoBattery,
            ui_icoIdleDisplay,
            ui_icoIdleSleep,
            ui_icoShutdown
    };

    apply_theme_list_panel(ui_pnlShutdown);
    apply_theme_list_panel(ui_pnlBattery);
    apply_theme_list_panel(ui_pnlIdleDisplay);
    apply_theme_list_panel(ui_pnlIdleSleep);

    apply_theme_list_item(&theme, ui_lblShutdown, lang.MUXPOWER.SLEEP.TITLE);
    apply_theme_list_item(&theme, ui_lblBattery, lang.MUXPOWER.LOW_BATTERY);
    apply_theme_list_item(&theme, ui_lblIdleDisplay, lang.MUXPOWER.IDLE.DISPLAY);
    apply_theme_list_item(&theme, ui_lblIdleSleep, lang.MUXPOWER.IDLE.SLEEP);

    apply_theme_list_glyph(&theme, ui_icoShutdown, mux_module, "shutdown");
    apply_theme_list_glyph(&theme, ui_icoBattery, mux_module, "battery");
    apply_theme_list_glyph(&theme, ui_icoIdleDisplay, mux_module, "idle_display");
    apply_theme_list_glyph(&theme, ui_icoIdleSleep, mux_module, "idle_sleep");

    apply_theme_list_drop_down(&theme, ui_droShutdown, NULL);

    char *battery_string = generate_number_string(5, 50, 5, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBattery_power, battery_string);
    free(battery_string);

    apply_theme_list_drop_down(&theme, ui_droIdleDisplay, "");
    apply_theme_list_drop_down(&theme, ui_droIdleSleep, "");

    char *sleep_timer[] = {
            lang.GENERIC.DISABLED, lang.MUXPOWER.SLEEP.SUSPEND, lang.MUXPOWER.SLEEP.INSTANT,
            lang.MUXPOWER.SLEEP.t10s, lang.MUXPOWER.SLEEP.t30s, lang.MUXPOWER.SLEEP.t60s,
            lang.MUXPOWER.SLEEP.t2m, lang.MUXPOWER.SLEEP.t5m, lang.MUXPOWER.SLEEP.t10m,
            lang.MUXPOWER.SLEEP.t15m, lang.MUXPOWER.SLEEP.t30m, lang.MUXPOWER.SLEEP.t60m
    };
    add_drop_down_options(ui_droShutdown, sleep_timer, SHUTDOWN_COUNT);

    char *idle_timer[] = {
            lang.GENERIC.DISABLED, lang.MUXPOWER.IDLE.t10s, lang.MUXPOWER.IDLE.t30s,
            lang.MUXPOWER.IDLE.t60s, lang.MUXPOWER.IDLE.t2m, lang.MUXPOWER.IDLE.t5m,
            lang.MUXPOWER.IDLE.t10m, lang.MUXPOWER.IDLE.t15m, lang.MUXPOWER.IDLE.t30m
    };
    add_drop_down_options(ui_droIdleDisplay, idle_timer, IDLE_COUNT);
    add_drop_down_options(ui_droIdleSleep, idle_timer, IDLE_COUNT);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
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

    play_sound(SND_BACK);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblShutdown, "shutdown");
    lv_obj_set_user_data(ui_lblBattery, "battery");
    lv_obj_set_user_data(ui_lblIdleDisplay, "idle_display");
    lv_obj_set_user_data(ui_lblIdleSleep, "idle_sleep");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

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
