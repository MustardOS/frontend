#include "muxshare.h"
#include "muxoption.h"
#include "ui/ui_muxoption.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"
#include "../common/log.h"

static char rom_name[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char rom_system[MAX_BUFFER_SIZE];

#define UI_COUNT 3
static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_icons[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblSearch_option,   lang.MUXOPTION.HELP.SEARCH},
            {ui_lblCore_option,     lang.MUXOPTION.HELP.ASSIGN_CORE},
            {ui_lblGovernor_option, lang.MUXOPTION.HELP.ASSIGN_GOV},
    };

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    if (element_focused == help_messages[0].element) {
        snprintf(message, sizeof(message), "%s", help_messages[0].message);
    } else if (element_focused == help_messages[1].element) {
        snprintf(message, sizeof(message), "%s\n\n%s:\n%s:  %s\n%s:  %s",
                 lang.MUXOPTION.HELP.ASSIGN_CORE,
                 lang.MUXOPTION.CURRENT,
                 lang.MUXOPTION.DIRECTORY, get_directory_core(rom_dir, 1),
                 lang.MUXOPTION.INDIVIDUAL, get_file_core(rom_dir, rom_name));
    } else if (element_focused == help_messages[2].element) {
        snprintf(message, sizeof(message), "%s\n\n%s:\n%s:  %s\n%s:  %s",
                 lang.MUXOPTION.HELP.ASSIGN_GOV,
                 lang.MUXOPTION.CURRENT,
                 lang.MUXOPTION.DIRECTORY, get_directory_governor(rom_dir),
                 lang.MUXOPTION.INDIVIDUAL, get_file_governor(rom_dir, rom_name));
    }

    if (strlen(message) <= 1) snprintf(message, sizeof(message), "%s", lang.GENERIC.NO_HELP);

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void add_info_item(int index, char *item_text, char *glyph_name, bool add_bottom_border) {
    lv_obj_t *ui_pnlInfoItem = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(ui_pnlInfoItem);
    lv_obj_t *ui_lblInfoItem = lv_label_create(ui_pnlInfoItem);
    apply_theme_list_item(&theme, ui_lblInfoItem, item_text);
    lv_obj_t *ui_icoInfoItem = lv_img_create(ui_pnlInfoItem);
    apply_theme_list_glyph(&theme, ui_icoInfoItem, mux_module, glyph_name);

    if (add_bottom_border) {
        lv_obj_set_height(ui_pnlInfoItem, 1);
        lv_obj_set_style_border_width(ui_pnlInfoItem, 1,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlInfoItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlInfoItem, theme.LIST_DEFAULT.TEXT_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlInfoItem, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_move_to_index(ui_pnlInfoItem, index);
}

static void add_info_items() {
    char game_directory[FILENAME_MAX];
    snprintf(game_directory, sizeof(game_directory), "%s:  %s", lang.MUXOPTION.DIRECTORY,
             get_last_subdir(rom_dir, '/', 4));
    add_info_item(0, game_directory, "folder", false);
    char game_name[FILENAME_MAX];
    snprintf(game_name, sizeof(game_name), "%s:  %s", lang.MUXOPTION.NAME, rom_name);
    add_info_item(1, game_name, "rom", false);
    add_info_item(2, "", "", true);
}

static void init_navigation_group() {
    add_info_items();
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlSearch_option,
            ui_pnlCore_option,
            ui_pnlGovernor_option
    };

    ui_objects[0] = ui_lblSearch_option;
    ui_objects[1] = ui_lblCore_option;
    ui_objects[2] = ui_lblGovernor_option;

    ui_icons[0] = ui_icoSearch_option;
    ui_icons[1] = ui_icoCore_option;
    ui_icons[2] = ui_icoGovernor_option;

    apply_theme_list_panel(ui_pnlSearch_option);
    apply_theme_list_panel(ui_pnlCore_option);
    apply_theme_list_panel(ui_pnlGovernor_option);

    apply_theme_list_item(&theme, ui_lblSearch_option, lang.MUXOPTION.SEARCH);
    apply_theme_list_item(&theme, ui_lblCore_option, lang.MUXOPTION.ASSIGN_CORE);
    apply_theme_list_item(&theme, ui_lblGovernor_option, lang.MUXOPTION.ASSIGN_GOV);

    apply_theme_list_glyph(&theme, ui_icoSearch_option, mux_module, "search");
    apply_theme_list_glyph(&theme, ui_icoCore_option, mux_module, "core");
    apply_theme_list_glyph(&theme, ui_icoGovernor_option, mux_module, "governor");

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_obj_set_user_data(ui_objects_panel[i], strdup(lv_label_get_text(ui_objects[i])));
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);

        apply_size_to_content(&theme, ui_pnlContent, ui_objects[i], ui_icons[i], lv_label_get_text(ui_objects[i]));
        apply_text_long_dot(&theme, ui_pnlContent, ui_objects[i], lv_label_get_text(ui_objects[i]));
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? UI_COUNT - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_confirm() {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"search",   "search",   &kiosk.CONTENT.SEARCH},
            {"core",     "assign",   &kiosk.CONTENT.ASSIGN_CORE},
            {"governor", "governor", &kiosk.CONTENT.ASSIGN_GOVERNOR}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                play_sound(SND_ERROR, 0);
                toast_message(kiosk_nope(), 1000, 1000);
                refresh_screen(ui_screen);
                return;
            }

            play_sound(SND_CONFIRM, 0);
            load_mux(elements[i].mux_name);
            break;
        }
    }

    close_input();
    mux_input_stop();
}

static void handle_back() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);

    remove(MUOS_SAA_LOAD);
    remove(MUOS_SAG_LOAD);

    close_input();
    mux_input_stop();
}

static void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
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

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblSearch_option, "search");
    lv_obj_set_user_data(ui_lblCore_option, "core");
    lv_obj_set_user_data(ui_lblGovernor_option, "governor");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxoption_main(int nothing, char *name, char *dir, char *sys) {
    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_dir, sizeof(rom_name), "%s", dir);
    snprintf(rom_system, sizeof(rom_name), "%s", sys);

    init_module("muxoption");

    if (file_exist(OPTION_SKIP)) {
        remove(OPTION_SKIP);
        LOG_INFO(mux_module, "Skipping Options Module - Not Required...")
        close_input();
        return 0;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOPTION.TITLE);
    init_muxoption(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
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
