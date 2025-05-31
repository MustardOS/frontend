#include "muxshare.h"
#include "muxtweakgen.h"
#include "ui/ui_muxtweakgen.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int startup_original, colour_original, brightness_original, volume_original;

#define UI_COUNT 7
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblStartup_tweakgen,    lang.MUXTWEAKGEN.HELP.STARTUP},
            {ui_lblColour_tweakgen,     lang.MUXTWEAKGEN.HELP.TEMP},
            {ui_lblRTC_tweakgen,        lang.MUXTWEAKGEN.HELP.DATETIME},
            {ui_lblBrightness_tweakgen, lang.MUXTWEAKGEN.HELP.BRIGHT},
            {ui_lblVolume_tweakgen,     lang.MUXTWEAKGEN.HELP.VOLUME},
            {ui_lblHDMI_tweakgen,       lang.MUXTWEAKGEN.HELP.HDMI},
            {ui_lblAdvanced_tweakgen,   lang.MUXTWEAKGEN.HELP.ADVANCED},
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
    startup_original = lv_dropdown_get_selected(ui_droStartup_tweakgen);
    colour_original = lv_dropdown_get_selected(ui_droColour_tweakgen);
    brightness_original = lv_dropdown_get_selected(ui_droBrightness_tweakgen);
    volume_original = lv_dropdown_get_selected(ui_droVolume_tweakgen);
}

static void update_volume_and_brightness() {
    char buffer[MAX_BUFFER_SIZE];
    CFG_INT_FIELD(config.SETTINGS.GENERAL.BRIGHTNESS, "settings/general/brightness", 90)
    CFG_INT_FIELD(config.SETTINGS.GENERAL.VOLUME, "settings/general/volume", 75)

    lv_dropdown_set_selected(ui_droBrightness_tweakgen, config.SETTINGS.GENERAL.BRIGHTNESS - 1);

    if (!config.SETTINGS.ADVANCED.OVERDRIVE) {
        lv_dropdown_set_selected(ui_droVolume_tweakgen, config.SETTINGS.GENERAL.VOLUME > 100
                                                        ? config.SETTINGS.GENERAL.VOLUME / 2
                                                        : config.SETTINGS.GENERAL.VOLUME);
    } else {
        lv_dropdown_set_selected(ui_droVolume_tweakgen, config.SETTINGS.GENERAL.VOLUME);
    }
}

static void restore_tweak_options() {
    update_volume_and_brightness();

    lv_dropdown_set_selected(ui_droColour_tweakgen, config.SETTINGS.GENERAL.COLOUR + 255);

    const char *startup_type = config.SETTINGS.GENERAL.STARTUP;
    if (strcasecmp(startup_type, "explore") == 0) {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 1);
    } else if (strcasecmp(startup_type, "collection") == 0) {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 2);
    } else if (strcasecmp(startup_type, "history") == 0) {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 3);
    } else if (strcasecmp(startup_type, "last") == 0) {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 4);
    } else if (strcasecmp(startup_type, "resume") == 0) {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 5);
    } else {
        lv_dropdown_set_selected(ui_droStartup_tweakgen, 0);
    }
}

static void set_setting_value(const char *script_name, int value, int offset) {
    char script_path[MAX_BUFFER_SIZE];
    snprintf(script_path, sizeof(script_path), INTERNAL_PATH "device/input/%s.sh", script_name);

    char value_str[8];
    snprintf(value_str, sizeof(value_str), "%d", value + offset);

    if (!block_input) {
        block_input = 1;
        const char *args[] = {script_path, value_str, NULL};
        run_exec(args, A_SIZE(args), 0);
        block_input = 0;
    }
}

static void save_tweak_options() {
    char *idx_startup;
    switch (lv_dropdown_get_selected(ui_droStartup_tweakgen)) {
        case 0:
            idx_startup = "launcher";
            break;
        case 1:
            idx_startup = "explore";
            break;
        case 2:
            idx_startup = "collection";
            break;
        case 3:
            idx_startup = "history";
            break;
        case 4:
            idx_startup = "last";
            break;
        case 5:
            idx_startup = "resume";
            break;
        default:
            idx_startup = "launcher";
            break;
    }

    int idx_brightness = lv_dropdown_get_selected(ui_droBrightness_tweakgen);
    int idx_volume = lv_dropdown_get_selected(ui_droVolume_tweakgen);
    int idx_colour = lv_dropdown_get_selected(ui_droColour_tweakgen) - 255;

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droStartup_tweakgen) != startup_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/general/startup"), "w", CHAR, idx_startup);
    }

    if (lv_dropdown_get_selected(ui_droColour_tweakgen) != colour_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/general/colour"), "w", INT, idx_colour);
    }

    if (lv_dropdown_get_selected(ui_droBrightness_tweakgen) != brightness_original) {
        is_modified++;
        set_setting_value("bright.sh", idx_brightness, 1);
    }

    if (lv_dropdown_get_selected(ui_droVolume_tweakgen) != volume_original) {
        is_modified++;
        set_setting_value("audio.sh", idx_volume, 0);
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
            ui_pnlRTC_tweakgen,
            ui_pnlHDMI_tweakgen,
            ui_pnlAdvanced_tweakgen,
            ui_pnlBrightness_tweakgen,
            ui_pnlVolume_tweakgen,
            ui_pnlColour_tweakgen,
            ui_pnlStartup_tweakgen
    };

    ui_objects[0] = ui_lblRTC_tweakgen;
    ui_objects[1] = ui_lblHDMI_tweakgen;
    ui_objects[2] = ui_lblAdvanced_tweakgen;
    ui_objects[3] = ui_lblBrightness_tweakgen;
    ui_objects[4] = ui_lblVolume_tweakgen;
    ui_objects[5] = ui_lblColour_tweakgen;
    ui_objects[6] = ui_lblStartup_tweakgen;

    lv_obj_t *ui_objects_value[] = {
            ui_droRTC_tweakgen,
            ui_droHDMI_tweakgen,
            ui_droAdvanced_tweakgen,
            ui_droBrightness_tweakgen,
            ui_droVolume_tweakgen,
            ui_droColour_tweakgen,
            ui_droStartup_tweakgen
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoRTC_tweakgen,
            ui_icoHDMI_tweakgen,
            ui_icoAdvanced_tweakgen,
            ui_icoBrightness_tweakgen,
            ui_icoVolume_tweakgen,
            ui_icoColour_tweakgen,
            ui_icoStartup_tweakgen
    };

    apply_theme_list_panel(ui_pnlStartup_tweakgen);
    apply_theme_list_panel(ui_pnlColour_tweakgen);
    apply_theme_list_panel(ui_pnlRTC_tweakgen);
    apply_theme_list_panel(ui_pnlBrightness_tweakgen);
    apply_theme_list_panel(ui_pnlVolume_tweakgen);
    apply_theme_list_panel(ui_pnlHDMI_tweakgen);
    apply_theme_list_panel(ui_pnlAdvanced_tweakgen);

    apply_theme_list_item(&theme, ui_lblStartup_tweakgen, lang.MUXTWEAKGEN.STARTUP.TITLE);
    apply_theme_list_item(&theme, ui_lblColour_tweakgen, lang.MUXTWEAKGEN.TEMP);
    apply_theme_list_item(&theme, ui_lblRTC_tweakgen, lang.MUXTWEAKGEN.DATETIME);
    apply_theme_list_item(&theme, ui_lblBrightness_tweakgen, lang.MUXTWEAKGEN.BRIGHT);
    apply_theme_list_item(&theme, ui_lblVolume_tweakgen, lang.MUXTWEAKGEN.VOLUME);
    apply_theme_list_item(&theme, ui_lblHDMI_tweakgen, lang.MUXTWEAKGEN.HDMI);
    apply_theme_list_item(&theme, ui_lblAdvanced_tweakgen, lang.MUXTWEAKGEN.ADVANCED);

    apply_theme_list_glyph(&theme, ui_icoStartup_tweakgen, mux_module, "startup");
    apply_theme_list_glyph(&theme, ui_icoColour_tweakgen, mux_module, "colour");
    apply_theme_list_glyph(&theme, ui_icoRTC_tweakgen, mux_module, "clock");
    apply_theme_list_glyph(&theme, ui_icoBrightness_tweakgen, mux_module, "brightness");
    apply_theme_list_glyph(&theme, ui_icoVolume_tweakgen, mux_module, "volume");
    apply_theme_list_glyph(&theme, ui_icoHDMI_tweakgen, mux_module, "hdmi");
    apply_theme_list_glyph(&theme, ui_icoAdvanced_tweakgen, mux_module, "advanced");

    apply_theme_list_drop_down(&theme, ui_droStartup_tweakgen, NULL);
    apply_theme_list_drop_down(&theme, ui_droColour_tweakgen, NULL);

    char *brightness_string = generate_number_string(1, device.SCREEN.BRIGHT, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBrightness_tweakgen, brightness_string);
    free(brightness_string);

    char *volume_string = generate_number_string(device.AUDIO.MIN, device.AUDIO.MAX, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droVolume_tweakgen, volume_string);
    free(volume_string);

    char *colour_string = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droColour_tweakgen, colour_string);
    free(colour_string);

    apply_theme_list_drop_down(&theme, ui_droRTC_tweakgen, NULL);
    apply_theme_list_drop_down(&theme, ui_droHDMI_tweakgen, NULL);
    apply_theme_list_drop_down(&theme, ui_droAdvanced_tweakgen, NULL);

    add_drop_down_options(ui_droStartup_tweakgen, (char *[]) {
            lang.MUXTWEAKGEN.STARTUP.MENU, lang.MUXTWEAKGEN.STARTUP.EXPLORE, lang.MUXTWEAKGEN.STARTUP.COLLECTION,
            lang.MUXTWEAKGEN.STARTUP.HISTORY, lang.MUXTWEAKGEN.STARTUP.LAST, lang.MUXTWEAKGEN.STARTUP.RESUME}, 6);

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

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblRTC_tweakgen || element_focused == ui_lblAdvanced_tweakgen) {
        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavLR, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavLRGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void update_option_values() {
    int curr_brightness = lv_dropdown_get_selected(ui_droBrightness_tweakgen);
    int curr_volume = lv_dropdown_get_selected(ui_droVolume_tweakgen);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblBrightness_tweakgen) {
        set_setting_value("bright", curr_brightness, 1);
    } else if (element_focused == ui_lblVolume_tweakgen) {
        set_setting_value("audio", curr_volume, 0);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
    update_option_values();
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
    update_option_values();
}

static void handle_confirm(void) {
    if (msgbox_active || block_input) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"clock",    "rtc",      &kiosk.DATETIME.CLOCK},
            {"hdmi",     "hdmi",     &kiosk.SETTING.HDMI},
            {"advanced", "tweakadv", &kiosk.SETTING.ADVANCED}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                play_sound(SND_ERROR);
                toast_message(kiosk_nope(), 1000);
                refresh_screen(ui_screen);
                return;
            }

            play_sound(SND_CONFIRM);

            save_tweak_options();
            load_mux(elements[i].mux_name);

            close_input();
            mux_input_stop();

            break;
        }
    }

    handle_option_next();
}

static void handle_back(void) {
    if (block_input) return;

    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || block_input) return;

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
    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblStartup_tweakgen, "startup");
    lv_obj_set_user_data(ui_lblColour_tweakgen, "colour");
    lv_obj_set_user_data(ui_lblRTC_tweakgen, "clock");
    lv_obj_set_user_data(ui_lblBrightness_tweakgen, "brightness");
    lv_obj_set_user_data(ui_lblVolume_tweakgen, "volume");
    lv_obj_set_user_data(ui_lblHDMI_tweakgen, "hdmi");
    lv_obj_set_user_data(ui_lblAdvanced_tweakgen, "advanced");

    if (!device.DEVICE.HAS_HDMI) {
        lv_obj_add_flag(ui_pnlHDMI_tweakgen, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
    }

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

int muxtweakgen_main() {
    init_module("muxtweakgen");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKGEN.TITLE);
    init_muxtweakgen(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_tweak_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

    init_timer(ui_refresh_task, update_volume_and_brightness);

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
