#include "muxpower.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxpower.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input/list_nav.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

int shutdown_original, battery_original, idle_display_original, idle_sleep_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 4
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

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

void show_help(lv_obj_t *element_focused) {
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

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void init_element_events() {
    lv_obj_t *dropdowns[] = {
            ui_droShutdown,
            ui_droBattery,
            ui_droIdleDisplay,
            ui_droIdleSleep
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

void init_dropdown_settings() {
    shutdown_original = lv_dropdown_get_selected(ui_droShutdown);
    battery_original = lv_dropdown_get_selected(ui_droBattery);
    idle_display_original = lv_dropdown_get_selected(ui_droIdleDisplay);
    idle_sleep_original = lv_dropdown_get_selected(ui_droIdleSleep);
}

void restore_tweak_options() {
    lv_obj_t *ui_droIdle[2] = {ui_droIdleDisplay, ui_droIdleSleep};
    int16_t *config_values[2] = {&config.SETTINGS.POWER.IDLE_DISPLAY, &config.SETTINGS.POWER.IDLE_SLEEP};

    map_drop_down_to_index(ui_droShutdown, config.SETTINGS.POWER.SHUTDOWN, shutdown_values, SHUTDOWN_COUNT, 0);
    map_drop_down_to_index(ui_droBattery, config.SETTINGS.POWER.LOW_BATTERY, battery_values, BATTERY_COUNT, 5);

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

void save_tweak_options() {
    int idx_shutdown = map_drop_down_to_value(lv_dropdown_get_selected(ui_droShutdown),
                                              shutdown_values, SHUTDOWN_COUNT, -2);

    int idx_battery = map_drop_down_to_value(lv_dropdown_get_selected(ui_droBattery),
                                             battery_values, BATTERY_COUNT, 25);

    int idx_idle_display = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleDisplay),
                                                  idle_values, IDLE_COUNT, 0);

    int idx_idle_sleep = map_drop_down_to_value(lv_dropdown_get_selected(ui_droIdleSleep),
                                                idle_values, IDLE_COUNT, 0);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droShutdown) != shutdown_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/power/shutdown"), "w", INT, idx_shutdown);
    }

    if (lv_dropdown_get_selected(ui_droBattery) != battery_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/power/low_battery"), "w", INT, idx_battery);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleDisplay) > 1 &&
        lv_dropdown_get_selected(ui_droIdleDisplay) != idle_display_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/power/idle_display"), "w", INT, idx_idle_display);
    }

    if (lv_dropdown_get_option_cnt(ui_droIdleSleep) > 1 &&
        lv_dropdown_get_selected(ui_droIdleSleep) != idle_sleep_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/power/idle_sleep"), "w", INT, idx_idle_sleep);
    }

    if (is_modified > 0) {
        static char tweak_script[MAX_BUFFER_SIZE];
        snprintf(tweak_script, sizeof(tweak_script),
                 "%s/script/mux/tweak.sh", INTERNAL_PATH);
        run_exec((const char *[]) {tweak_script, NULL});
    }
}

void init_navigation_group() {
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
            ui_droBattery,
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
    apply_theme_list_drop_down(&theme, ui_droBattery, battery_string);
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

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "power");

    safe_quit(0);
    mux_input_stop();
}

void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
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

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxpower_main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(1, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXPOWER.TITLE);
    init_mux(ui_pnlContent);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_element_events();
    init_navigation_sound(&nav_sound, mux_module);

    restore_tweak_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);

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
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
